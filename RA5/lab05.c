/*
Charles Andrei P. De los Reyes
2023-15797
B-3L

Lab 05: Distributed Min-Max Transformation (column-wise)
- Master 1MPB-broadcasts the full matrix X to slaves via tree (O(log t)).
- Each slave computes MMT on its assigned column range only.
- Slaves M1PR-reduce their T-strips back up the same tree.
- Master rebuilds the full T.

Master timer: full distribute -> rebuild round-trip (Table 1).
Slave  timer: own MMT compute window only (Table 2 — report max across slaves).
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <float.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sched.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define Sleep(ms) usleep((ms) * 1000)
#endif

static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

int send_all(SOCKET sock, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        int sent = send(sock, buf + total, (int)(len - total), 0);
        if (sent <= 0) return -1;
        total += (size_t)sent;
    }
    return (int)total;
}

int recv_all(SOCKET sock, char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        int got = recv(sock, buf + total, (int)(len - total), 0);
        if (got <= 0) return -1;
        total += (size_t)got;
    }
    return (int)total;
}

// Column count assigned to slaves [first, first+count) under base = n/t, rem = n%t.
int compute_size_for_range(int n, int t, int first, int count) {
    int base = n / t, rem = n % t, total = 0;
    for (int i = first; i < first + count; i++)
        total += base + (i < rem ? 1 : 0);
    return total;
}

// MMT on absolute columns [col_first, col_first+col_count) of an n*n int matrix X (row-major).
// Writes results into T_strip with row stride `strip_stride`, i.e. T_strip[r*strip_stride + j].
// The leftmost `col_count` entries per row are populated; columns beyond that are untouched.
void compute_mmt_strip(const int *X, int n, int col_first, int col_count,
                       float *T_strip, int strip_stride) {
    for (int j = 0; j < col_count; j++) {
        int abs_c = col_first + j;
        float vmin = FLT_MAX, vmax = -FLT_MAX;
        for (int r = 0; r < n; r++) {
            float v = (float)X[r * n + abs_c];
            if (v < vmin) vmin = v;
            if (v > vmax) vmax = v;
        }
        float denom = vmax - vmin;
        if (denom != 0.0f) {
            for (int r = 0; r < n; r++) {
                float v = (float)X[r * n + abs_c];
                T_strip[r * strip_stride + j] = (v - vmin) / denom;
            }
        } else {
            for (int r = 0; r < n; r++)
                T_strip[r * strip_stride + j] = 0.0f;
        }
    }
}

typedef struct {
    int *X;                    // full matrix to send (n*n ints) — shared, not freed by worker
    int n;
    int col_first, col_count;  // subtree's column-range (work assignment)
    int first_slave, num_slaves;
    int t_id;                  // 1-based slave id of subtree root (display only)
    char ip[64];
    int port;
    int success;
    float *returned_strip;     // worker mallocs n*col_count floats; caller frees
} TArgs;

// thread: connect to subtree root, send X + work range, recv T-strip
void *worker_func(void *args) {
    TArgs *a = (TArgs *)args;
    a->success = 0;
    a->returned_strip = NULL;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return NULL;

    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((unsigned short)a->port);
    inet_pton(AF_INET, a->ip, &saddr.sin_addr);

    int connected = 0;
    for (int i = 0; i < 30 && !connected; i++) {
        if (connect(sock, (struct sockaddr *)&saddr, sizeof(saddr)) == 0)
            connected = 1;
        else
            Sleep(500);
    }
    if (!connected) { closesocket(sock); return NULL; }

    // metadata
    send_all(sock, (char *)&a->n, sizeof(int));
    send_all(sock, (char *)&a->col_first, sizeof(int));
    send_all(sock, (char *)&a->col_count, sizeof(int));
    send_all(sock, (char *)&a->first_slave, sizeof(int));
    send_all(sock, (char *)&a->num_slaves, sizeof(int));

    // full X
    send_all(sock, (char *)a->X, (size_t)a->n * a->n * sizeof(int));

    if (a->n <= 32) {
        pthread_mutex_lock(&print_lock);
        printf("[-> Slave %d] SENT X + work [cols %d..%d] (subtree of %d slave(s))\n",
               a->t_id, a->col_first, a->col_first + a->col_count - 1, a->num_slaves);
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    // recv T-strip from subtree
    size_t strip_bytes = (size_t)a->n * a->col_count * sizeof(float);
    a->returned_strip = (float *)malloc(strip_bytes);
    if (a->returned_strip && recv_all(sock, (char *)a->returned_strip, strip_bytes) >= 0)
        a->success = 1;

    closesocket(sock);
    return NULL;
}

int run_master(int n, int p, char *config_file, char *input_file) {
    (void)p;
    FILE *cfg = fopen(config_file, "r");
    if (!cfg) { perror("config"); return 1; }

    char slave_ips[64][64];
    int slave_ports[64], t = 0;
    char line[256];

    while (fgets(line, 256, cfg)) {
        char role[16], ip[64]; int port;
        if (sscanf(line, "%15s %63s %d", role, ip, &port) != 3) continue;
        if (strcmp(role, "slave") == 0 && t < 64) {
            strncpy(slave_ips[t], ip, 63);
            slave_ports[t] = port;
            t++;
        }
    }
    fclose(cfg);
    if (t == 0) { printf("No slaves in config.\n"); return 1; }

    int file_mode = (input_file != NULL);
    printf("[Master] n=%d, slaves=%d, mode=%s\n", n, t, file_mode ? "file" : "random");

    int *X;
    if (file_mode) {
        FILE *fp = fopen(input_file, "r");
        if (!fp) { perror("input"); return 1; }
        int file_n;
        fscanf(fp, "%d", &file_n);
        if (file_n != n) { printf("[Master] file n=%d overrides arg n=%d\n", file_n, n); n = file_n; }
        X = (int *)malloc((size_t)n * n * sizeof(int));
        for (int k = 0; k < n * n; k++) {
            fscanf(fp, "%d", &X[k]);
            int ch = fgetc(fp); if (ch != ',') ungetc(ch, fp);
        }
        fclose(fp);
    } else {
        X = (int *)malloc((size_t)n * n * sizeof(int));
        srand(time(NULL));
        for (int k = 0; k < n * n; k++)
            X[k] = (rand() % 100) + 1;
    }

    if (file_mode || n <= 32) {
        printf("\n--- Full Matrix X (%d x %d) ---\n", n, n);
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) printf("%d\t", X[r * n + c]);
            printf("\n");
        }
    }

    int curr_c = 0;
    for (int i = 0; i < t; i++) {
        int assigned = n / t + (i < n % t ? 1 : 0);
        printf("[Master] Slave %d (%s:%d) -> %d cols (cols %d to %d)\n",
               i + 1, slave_ips[i], slave_ports[i], assigned, curr_c, curr_c + assigned - 1);
        curr_c += assigned;
    }

    // build tree direct-children list (recursive halving by column-range)
    TArgs t_args[64];
    int num_children = 0;
    int rem_first = 0, rem_count = t;
    int rem_col_first = 0, rem_col_count = n;

    while (rem_count > 0) {
        if (rem_count == 1) {
            t_args[num_children].X = X;
            t_args[num_children].n = n;
            t_args[num_children].col_first = rem_col_first;
            t_args[num_children].col_count = rem_col_count;
            t_args[num_children].first_slave = rem_first;
            t_args[num_children].num_slaves = 1;
            t_args[num_children].t_id = rem_first + 1;
            strncpy(t_args[num_children].ip, slave_ips[rem_first], 63);
            t_args[num_children].port = slave_ports[rem_first];
            num_children++;
            break;
        }
        int left_count = rem_count / 2;
        int right_count = rem_count - left_count;
        int right_first = rem_first + left_count;
        int left_cols = compute_size_for_range(n, t, rem_first, left_count);
        int right_cols = rem_col_count - left_cols;

        t_args[num_children].X = X;
        t_args[num_children].n = n;
        t_args[num_children].col_first = rem_col_first + left_cols;
        t_args[num_children].col_count = right_cols;
        t_args[num_children].first_slave = right_first;
        t_args[num_children].num_slaves = right_count;
        t_args[num_children].t_id = right_first + 1;
        strncpy(t_args[num_children].ip, slave_ips[right_first], 63);
        t_args[num_children].port = slave_ports[right_first];
        num_children++;

        rem_count = left_count;
        rem_col_count = left_cols;
    }

    printf("\n[Master] Tree: %d direct children (O(log %d))\n", num_children, t);

    // sort by t_id (cosmetic: prints follow slave numbering)
    for (int i = 0; i < num_children - 1; i++)
        for (int j = i + 1; j < num_children; j++)
            if (t_args[i].t_id > t_args[j].t_id) {
                TArgs tmp = t_args[i]; t_args[i] = t_args[j]; t_args[j] = tmp;
            }

    float *T = (float *)malloc((size_t)n * n * sizeof(float));
    if (!T) { fprintf(stderr, "[Master] alloc T failed\n"); free(X); return 1; }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t threads[64];
    for (int i = 0; i < num_children; i++)
        pthread_create(&threads[i], NULL, worker_func, &t_args[i]);

    for (int i = 0; i < num_children; i++) {
        pthread_join(threads[i], NULL);
        if (t_args[i].success && t_args[i].returned_strip) {
            int cf = t_args[i].col_first, cc = t_args[i].col_count;
            for (int r = 0; r < n; r++)
                memcpy(&T[r * n + cf], &t_args[i].returned_strip[r * cc], (size_t)cc * sizeof(float));
            free(t_args[i].returned_strip);
            t_args[i].returned_strip = NULL;
            pthread_mutex_lock(&print_lock);
            printf("[Master] Received T-strip from Slave %d (cols %d..%d, subtree=%d)\n",
                   t_args[i].t_id, cf, cf + cc - 1, t_args[i].num_slaves);
            fflush(stdout);
            pthread_mutex_unlock(&print_lock);
        } else {
            fprintf(stderr, "[Master] FAILED to receive from Slave %d\n", t_args[i].t_id);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("time elapsed: %.6f seconds\n", time_elapsed);

    if (file_mode || n <= 32) {
        printf("\n--- Full Matrix T (%d x %d) ---\n", n, n);
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) printf("%.4f\t", T[r * n + c]);
            printf("\n");
        }
    }

    free(X);
    free(T);
    return 0;
}

int run_slave(int n, int p, char *config_file, int core_id) {
    (void)n;

    int num_cores;
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        num_cores = (int)sysinfo.dwNumberOfProcessors;
    #else
        num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
        if (num_cores < 1) num_cores = 1;
    #endif
    printf("[Slave] Detected %d CPU core(s) on this machine (valid core_id: 0 to %d)\n",
           num_cores, num_cores - 1);

    if (core_id >= num_cores) {
        fprintf(stderr, "[Slave] ERROR: core_id %d is out of range; this machine only has %d core(s) (max valid id = %d).\n",
                core_id, num_cores, num_cores - 1);
        return 1;
    }

    if (core_id >= 0) {
        #ifdef _WIN32
            SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)1 << core_id);
        #else
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);
            sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
        #endif
        printf("[Slave] Pinned to core %d\n", core_id);
    }

    FILE *cfg = fopen(config_file, "r");
    if (!cfg) { perror("config"); return 1; }

    char slave_ips[64][64];
    int slave_ports[64], t = 0, slave_id = 1;
    char line[256];

    while (fgets(line, 256, cfg)) {
        char role[16], ip[64]; int port;
        if (sscanf(line, "%15s %63s %d", role, ip, &port) != 3) continue;
        if (strcmp(role, "slave") == 0 && t < 64) {
            strncpy(slave_ips[t], ip, 63);
            slave_ports[t] = port;
            if (port == p) slave_id = t + 1;
            t++;
        }
    }
    fclose(cfg);

    printf("[Slave %d] Listening on port %d...\n", slave_id, p);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)p);
    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv, 1);

    SOCKET csock = accept(srv, NULL, NULL);
    printf("[Slave %d] Connected by parent.\n", slave_id);

    // recv metadata
    int recv_n, col_first, col_count, first_slave, num_slaves;
    recv_all(csock, (char *)&recv_n, sizeof(int));
    recv_all(csock, (char *)&col_first, sizeof(int));
    recv_all(csock, (char *)&col_count, sizeof(int));
    recv_all(csock, (char *)&first_slave, sizeof(int));
    recv_all(csock, (char *)&num_slaves, sizeof(int));

    // recv full X
    size_t X_bytes = (size_t)recv_n * recv_n * sizeof(int);
    int *X = (int *)malloc(X_bytes);
    if (!X) { fprintf(stderr, "[Slave %d] alloc X failed\n", slave_id); closesocket(csock); closesocket(srv); return 1; }
    recv_all(csock, (char *)X, X_bytes);

    // own MMT range = leftmost slave's columns within the subtree
    int my_col_first = col_first;
    int my_col_count = recv_n / t + (first_slave < recv_n % t ? 1 : 0);

    printf("[Slave %d] RECEIVED X (n=%d), assignment cols [%d..%d] (subtree=%d), my MMT cols [%d..%d]\n",
           slave_id, recv_n, col_first, col_first + col_count - 1, num_slaves,
           my_col_first, my_col_first + my_col_count - 1);

    if (recv_n <= 32) {
        pthread_mutex_lock(&print_lock);
        printf("[Slave %d] X received (%d x %d):\n", slave_id, recv_n, recv_n);
        for (int r = 0; r < recv_n; r++) {
            for (int c = 0; c < recv_n; c++) printf("%d\t", X[r * recv_n + c]);
            printf("\n");
        }
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    // build child forwarders (subtree split mirrors master's)
    TArgs child_args[64];
    pthread_t child_threads[64];
    int num_children = 0;

    if (num_slaves > 1) {
        int rem_first = first_slave, rem_count = num_slaves;
        int rem_col_first = col_first, rem_col_count = col_count;

        while (rem_count > 1) {
            int left_count = rem_count / 2;
            int right_count = rem_count - left_count;
            int right_first = rem_first + left_count;
            int left_cols = compute_size_for_range(recv_n, t, rem_first, left_count);
            int right_cols = rem_col_count - left_cols;

            child_args[num_children].X = X;
            child_args[num_children].n = recv_n;
            child_args[num_children].col_first = rem_col_first + left_cols;
            child_args[num_children].col_count = right_cols;
            child_args[num_children].first_slave = right_first;
            child_args[num_children].num_slaves = right_count;
            child_args[num_children].t_id = right_first + 1;
            strncpy(child_args[num_children].ip, slave_ips[right_first], 63);
            child_args[num_children].port = slave_ports[right_first];
            num_children++;

            rem_count = left_count;
            rem_col_count = left_cols;
        }

        // sort by t_id (cosmetic)
        for (int i = 0; i < num_children - 1; i++)
            for (int j = i + 1; j < num_children; j++)
                if (child_args[i].t_id > child_args[j].t_id) {
                    TArgs tmp = child_args[i]; child_args[i] = child_args[j]; child_args[j] = tmp;
                }

        // launch forwarders BEFORE timer — communication overlap, not part of compute
        for (int i = 0; i < num_children; i++) {
            pthread_mutex_lock(&print_lock);
            printf("[Slave %d] Forwarding to Slave %d: cols [%d..%d], subtree=%d\n",
                   slave_id, child_args[i].t_id,
                   child_args[i].col_first, child_args[i].col_first + child_args[i].col_count - 1,
                   child_args[i].num_slaves);
            fflush(stdout);
            pthread_mutex_unlock(&print_lock);
            pthread_create(&child_threads[i], NULL, worker_func, &child_args[i]);
        }
    }

    // allocate the subtree's combined T-strip; own MMT writes into the leftmost portion
    size_t strip_bytes = (size_t)recv_n * col_count * sizeof(float);
    float *subtree_strip = (float *)malloc(strip_bytes);
    if (!subtree_strip) { fprintf(stderr, "[Slave %d] alloc subtree_strip failed\n", slave_id); free(X); closesocket(csock); closesocket(srv); return 1; }

    // === COMPUTE WINDOW (slave timer) ===
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    compute_mmt_strip(X, recv_n, my_col_first, my_col_count, subtree_strip, col_count);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("time elapsed: %.6f seconds\n", time_elapsed);
    // === end timer ===

    if (recv_n <= 32) {
        pthread_mutex_lock(&print_lock);
        printf("[Slave %d] My MMT (cols [%d..%d]):\n",
               slave_id, my_col_first, my_col_first + my_col_count - 1);
        for (int r = 0; r < recv_n; r++) {
            for (int j = 0; j < my_col_count; j++)
                printf("%.4f\t", subtree_strip[r * col_count + j]);
            printf("\n");
        }
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    // collect children's T-strips and place them into subtree_strip at the correct column offset
    for (int i = 0; i < num_children; i++) {
        pthread_join(child_threads[i], NULL);
        if (child_args[i].success && child_args[i].returned_strip) {
            int offset = child_args[i].col_first - col_first;
            int cc = child_args[i].col_count;
            for (int r = 0; r < recv_n; r++)
                memcpy(&subtree_strip[r * col_count + offset],
                       &child_args[i].returned_strip[r * cc],
                       (size_t)cc * sizeof(float));
            free(child_args[i].returned_strip);
            child_args[i].returned_strip = NULL;
            pthread_mutex_lock(&print_lock);
            printf("[Slave %d] Received T-strip from Slave %d (cols [%d..%d])\n",
                   slave_id, child_args[i].t_id,
                   child_args[i].col_first, child_args[i].col_first + cc - 1);
            fflush(stdout);
            pthread_mutex_unlock(&print_lock);
        } else {
            fprintf(stderr, "[Slave %d] FAILED to receive from Slave %d\n",
                    slave_id, child_args[i].t_id);
        }
    }

    if (recv_n <= 32 && num_slaves > 1) {
        pthread_mutex_lock(&print_lock);
        printf("[Slave %d] Subtree T-strip ready (cols [%d..%d]):\n",
               slave_id, col_first, col_first + col_count - 1);
        for (int r = 0; r < recv_n; r++) {
            for (int j = 0; j < col_count; j++)
                printf("%.4f\t", subtree_strip[r * col_count + j]);
            printf("\n");
        }
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    // send subtree T-strip up to parent
    send_all(csock, (char *)subtree_strip, strip_bytes);
    pthread_mutex_lock(&print_lock);
    printf("[Slave %d] Sent T-strip to parent (cols [%d..%d], %d slave(s) in subtree)\n",
           slave_id, col_first, col_first + col_count - 1, num_slaves);
    fflush(stdout);
    pthread_mutex_unlock(&print_lock);

    free(subtree_strip);
    free(X);
    closesocket(csock);
    closesocket(srv);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <n> <p> <s> <config_file> [input_file|core_id]\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int p = atoi(argv[2]);
    int s = atoi(argv[3]);
    char *config_file = argv[4];

    #ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    if (s == 0) {
        char *input_file = (argc >= 6) ? argv[5] : NULL;
        run_master(n, p, config_file, input_file);
    } else {
        int core_id = (argc >= 6) ? atoi(argv[5]) : -1;
        run_slave(n, p, config_file, core_id);
    }

    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}
