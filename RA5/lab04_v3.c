/*
Charles Andrei P. De los Reyes
2023-15797
B-3L
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

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

// global print lock so multi-line matrix prints from parallel threads
static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

int send_all(SOCKET sock, const char *buf, int len) {
    int total = 0;
    while (total < len) {
        int sent = send(sock, buf + total, len - total, 0);
        if (sent <= 0) return -1;
        total += sent;
    }
    return total;
}

int recv_all(SOCKET sock, char *buf, int len) {
    int total = 0;
    while (total < len) {
        int got = recv(sock, buf + total, len - total, 0);
        if (got <= 0) return -1;
        total += got;
    }
    return total;
}

// compute total rows for slaves [first, first+count) given n rows split among t slaves
int compute_rows_for_range(int n, int t, int first, int count) {
    int base = n / t, rem = n % t, total = 0;
    for (int i = first; i < first + count; i++)
        total += base + (i < rem ? 1 : 0);
    return total;
}

typedef struct {
    int *sub;
    int rows, n, start_row;
    int first_slave, num_slaves, t_id;
    char ip[64];
    int port;
    int success;
} TArgs;

// thread: connect to child slave, send metadata + data, receive tree ack
void *worker_func(void *args) {
    TArgs *a = (TArgs *)args;
    a->success = 0;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return NULL;

    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((unsigned short)a->port);
    inet_pton(AF_INET, a->ip, &saddr.sin_addr);

    // retry until child is ready
    int connected = 0;
    for (int i = 0; i < 30 && !connected; i++) {
        if (connect(sock, (struct sockaddr *)&saddr, sizeof(saddr)) == 0)
            connected = 1;
        else
            Sleep(500);
    }
    if (!connected) { closesocket(sock); return NULL; }

    // send metadata: n, rows, start_row, first_slave, num_slaves
    send_all(sock, (char *)&a->n, sizeof(int));
    send_all(sock, (char *)&a->rows, sizeof(int));
    send_all(sock, (char *)&a->start_row, sizeof(int));
    send_all(sock, (char *)&a->first_slave, sizeof(int));
    send_all(sock, (char *)&a->num_slaves, sizeof(int));
    send_all(sock, (char *)a->sub, a->rows * a->n * (int)sizeof(int));

    // after send: print the submatrix just sent (demo aid, small n only)
    if (a->n <= 32) {
        pthread_mutex_lock(&print_lock);
        printf("[-> Slave %d] SENT %d rows (rows %d to %d):\n",
               a->t_id, a->rows, a->start_row, a->start_row + a->rows - 1);
        for (int r = 0; r < a->rows; r++) {
            for (int c = 0; c < a->n; c++)
                printf("%d\t", a->sub[r * a->n + c]);
            printf("\n");
        }
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    // wait for tree ack from child (child acks after its subtree completes)
    char ack[4] = {0};
    if (recv_all(sock, ack, 3) >= 0)
        a->success = 1;

    closesocket(sock);
    return NULL;
}

int run_master(int n, int p, char *config_file, char *input_file) {
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

    // create or read matrix
    int *M;
    if (file_mode) {
        FILE *fp = fopen(input_file, "r");
        if (!fp) { perror("input"); return 1; }
        int file_n;
        fscanf(fp, "%d", &file_n);
        if (file_n != n) { printf("[Master] file n=%d overrides arg n=%d\n", file_n, n); n = file_n; }
        M = (int *)malloc((size_t)n * n * sizeof(int));
        for (int k = 0; k < n * n; k++) {
            fscanf(fp, "%d", &M[k]);
            int ch = fgetc(fp); if (ch != ',') ungetc(ch, fp);
        }
        fclose(fp);
    } else {
        M = (int *)malloc((size_t)n * n * sizeof(int));
        srand(time(NULL));
        for (int k = 0; k < n * n; k++)
            M[k] = (rand() % 100) + 1;
    }

    // print matrix for verification (small matrices or file mode)
    if (file_mode || n <= 32) {
        printf("\n--- Full Matrix M (%d x %d) ---\n", n, n);
        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) printf("%d\t", M[r * n + c]);
            printf("\n");
        }
    }

    // show slave assignments
    int curr_r = 0;
    for (int i = 0; i < t; i++) {
        int assigned = n / t + (i < n % t ? 1 : 0);
        printf("[Master] Slave %d (%s:%d) -> %d rows (rows %d to %d)\n",
               i + 1, slave_ips[i], slave_ports[i], assigned, curr_r, curr_r + assigned - 1);
        if (file_mode || n <= 32) {
            for (int r = 0; r < assigned; r++) {
                for (int c = 0; c < n; c++) printf("%d\t", M[(curr_r + r) * n + c]);
                printf("\n");
            }
        }
        curr_r += assigned;
    }

    // compute tree children: iteratively split range, keep left, send right
    TArgs t_args[32];
    int num_children = 0;
    int rem_first = 0, rem_count = t, rem_start_row = 0, rem_rows = n;

    while (rem_count > 0) {
        if (rem_count == 1) {
            t_args[num_children].sub = &M[(size_t)rem_start_row * n];
            t_args[num_children].rows = rem_rows;
            t_args[num_children].n = n;
            t_args[num_children].start_row = rem_start_row;
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
        int left_rows = compute_rows_for_range(n, t, rem_first, left_count);
        int right_rows = rem_rows - left_rows;

        t_args[num_children].sub = &M[(size_t)(rem_start_row + left_rows) * n];
        t_args[num_children].rows = right_rows;
        t_args[num_children].n = n;
        t_args[num_children].start_row = rem_start_row + left_rows;
        t_args[num_children].first_slave = right_first;
        t_args[num_children].num_slaves = right_count;
        t_args[num_children].t_id = right_first + 1;
        strncpy(t_args[num_children].ip, slave_ips[right_first], 63);
        t_args[num_children].port = slave_ports[right_first];
        num_children++;

        rem_count = left_count;
        rem_rows = left_rows;
    }

    printf("\n[Master] Tree: %d direct children (O(log %d))\n", num_children, t);

    // sort t_args by t_id so display order matches slave numbering (cosmetic)
    for (int i = 0; i < num_children - 1; i++) {
        for (int j = i + 1; j < num_children; j++) {
            if (t_args[i].t_id > t_args[j].t_id) {
                TArgs tmp = t_args[i]; t_args[i] = t_args[j]; t_args[j] = tmp;
            }
        }
    }

    // start timer, launch tree threads, join, stop timer
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t threads[32];
    for (int i = 0; i < num_children; i++)
        pthread_create(&threads[i], NULL, worker_func, &t_args[i]);

    for (int i = 0; i < num_children; i++) {
        pthread_join(threads[i], NULL);
        pthread_mutex_lock(&print_lock);
        printf("[Master] Received 'ack' from Slave %d (subtree: %d)\n",
               t_args[i].t_id, t_args[i].num_slaves);
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_elapsed = (end.tv_sec - start.tv_sec) +
                          (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("time elapsed: %.6f seconds\n", time_elapsed);

    free(M);
    return 0;
}

int run_slave(int n, int p, char *config_file, int core_id) {
    // detect available cores so the user knows the valid core_id range
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

    // reject out-of-range core_id before calling the pinning syscall
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

    // read config: need all slave IPs/ports for tree forwarding
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

    // listen for parent connection
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

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("[Slave %d] Connected by parent.\n", slave_id);

    // receive metadata + data
    int recv_n, rows, start_row, first_slave, num_slaves;
    recv_all(csock, (char *)&recv_n, sizeof(int));
    recv_all(csock, (char *)&rows, sizeof(int));
    recv_all(csock, (char *)&start_row, sizeof(int));
    recv_all(csock, (char *)&first_slave, sizeof(int));
    recv_all(csock, (char *)&num_slaves, sizeof(int));

    int sub_bytes = rows * recv_n * (int)sizeof(int);
    int *sub = (int *)malloc((size_t)sub_bytes);
    recv_all(csock, (char *)sub, sub_bytes);

    printf("[Slave %d] Received %d rows (rows %d to %d), covers %d slave(s)\n",
           slave_id, rows, start_row, start_row + rows - 1, num_slaves);

    // after recv: print the submatrix just received (demo aid, small n only)
    if (recv_n <= 32) {
        pthread_mutex_lock(&print_lock);
        printf("[Slave %d] RECEIVED %d x %d submatrix (rows %d to %d):\n",
               slave_id, rows, recv_n, start_row, start_row + rows - 1);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < recv_n; c++)
                printf("%d\t", sub[r * recv_n + c]);
            printf("\n");
        }
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    // tree forwarding: split and send to children if covering multiple slaves
    int num_children = 0;
    TArgs child_args[32];
    pthread_t child_threads[32];

    if (num_slaves > 1) {
        int rem_first = first_slave, rem_count = num_slaves;
        int rem_start = start_row, rem_rows = rows, data_offset = 0;

        // fill child_args (no thread launch yet — we want to sort first)
        while (rem_count > 1) {
            int left_count = rem_count / 2;
            int right_count = rem_count - left_count;
            int right_first = rem_first + left_count;
            int left_rows = compute_rows_for_range(recv_n, t, rem_first, left_count);
            int right_rows = rem_rows - left_rows;

            child_args[num_children].sub = sub + (size_t)(data_offset + left_rows) * recv_n;
            child_args[num_children].rows = right_rows;
            child_args[num_children].n = recv_n;
            child_args[num_children].start_row = rem_start + left_rows;
            child_args[num_children].first_slave = right_first;
            child_args[num_children].num_slaves = right_count;
            child_args[num_children].t_id = right_first + 1;
            strncpy(child_args[num_children].ip, slave_ips[right_first], 63);
            child_args[num_children].port = slave_ports[right_first];
            num_children++;

            rem_count = left_count;
            rem_rows = left_rows;
        }
        rows = rem_rows;
        start_row = rem_start;

        // sort child_args by t_id so display order matches slave numbering (cosmetic)
        for (int i = 0; i < num_children - 1; i++) {
            for (int j = i + 1; j < num_children; j++) {
                if (child_args[i].t_id > child_args[j].t_id) {
                    TArgs tmp = child_args[i]; child_args[i] = child_args[j]; child_args[j] = tmp;
                }
            }
        }

        // launch threads in sorted order
        for (int i = 0; i < num_children; i++) {
            pthread_mutex_lock(&print_lock);
            printf("[Slave %d] Forwarding %d rows to Slave %d\n",
                   slave_id, child_args[i].rows, child_args[i].t_id);
            fflush(stdout);
            pthread_mutex_unlock(&print_lock);
            pthread_create(&child_threads[i], NULL, worker_func, &child_args[i]);
        }
    }

    // print my final submatrix for verification
    if (recv_n <= 32) {
        pthread_mutex_lock(&print_lock);
        printf("[Slave %d] My submatrix (%d x %d, rows %d to %d):\n",
               slave_id, rows, recv_n, start_row, start_row + rows - 1);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < recv_n; c++) printf("%d\t", sub[r * recv_n + c]);
            printf("\n");
        }
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    // wait for children to ack before acking parent
    for (int i = 0; i < num_children; i++) {
        pthread_join(child_threads[i], NULL);
        pthread_mutex_lock(&print_lock);
        printf("[Slave %d] Received ack from Slave %d\n", slave_id, child_args[i].t_id);
        fflush(stdout);
        pthread_mutex_unlock(&print_lock);
    }

    // send tree ack to parent
    send_all(csock, "ack", 3);
    pthread_mutex_lock(&print_lock);
    printf("[Slave %d] Sent 'ack' to parent.\n", slave_id);
    fflush(stdout);
    pthread_mutex_unlock(&print_lock);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_elapsed = (end.tv_sec - start.tv_sec) +
                          (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("time elapsed: %.6f seconds\n", time_elapsed);

    free(sub);
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
