/*
Charles Andrei P. De los Reyes
2023-15797
B-3L
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// include windows api for socket and core affinity
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

// lock for clean printing
pthread_mutex_t console_lock = PTHREAD_MUTEX_INITIALIZER;

// send all bytes over socket
int send_all(SOCKET sock, const char *buf, int len) {
    int total = 0;
    while (total < len) {
        int sent = send(sock, buf + total, len - total, 0);
        if (sent == SOCKET_ERROR || sent == 0) return -1;
        total += sent;
    }
    return total;
}

// receive all bytes from socket
int recv_all(SOCKET sock, char *buf, int len) {
    int total = 0;
    while (total < len) {
        int got = recv(sock, buf + total, len - total, 0);
        if (got <= 0) return -1;
        total += got;
    }
    return total;
}

// compute total rows assigned to slaves [first, first+count)
// slave i (0-indexed) gets base + (i < rem ? 1 : 0) rows
int compute_rows_for_range(int n, int t, int first, int count) {
    int base = n / t;
    int rem = n % t;
    int total = 0;
    for (int i = first; i < first + count; i++) {
        total += base + (i < rem ? 1 : 0);
    }
    return total;
}

// print full matrix
void print_matrix(int *M, int n) {
    pthread_mutex_lock(&console_lock);
    printf("\n--- Full Matrix M (%d x %d) ---\n", n, n);
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            printf("%d\t", M[r * n + c]);
        }
        printf("\n");
    }
    printf("------------------------------------\n");
    pthread_mutex_unlock(&console_lock);
}

// print submatrix for a specific slave
void print_submatrix(int *sub, int rows, int cols, int t_id, int start_row) {
    pthread_mutex_lock(&console_lock);
    printf("\n--- Slave %d Submatrix (%d x %d, rows %d to %d) ---\n",
           t_id, rows, cols, start_row, start_row + rows - 1);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            printf("%d\t", sub[r * cols + c]);
        }
        printf("\n");
    }
    printf("------------------------------------\n");
    pthread_mutex_unlock(&console_lock);
}

// local mmt on received submatrix (reused from lab01, prep for lab05)
void mmt(int *sub_int, int rows, int cols, int t_id) {
    float *sub = (float *)malloc(rows * cols * sizeof(float));
    if (sub == NULL) return;

    // convert int to float
    for (int k = 0; k < rows * cols; k++) {
        sub[k] = (float)sub_int[k];
    }

    // column-wise mmt using local min/max
    for (int i = 0; i < cols; i++) {
        float min = sub[0 * cols + i];
        float max = min;

        for (int j = 1; j < rows; j++) {
            float val = sub[j * cols + i];
            if (val < min) min = val;
            if (val > max) max = val;
        }

        float denominator = max - min;

        for (int j = 0; j < rows; j++) {
            if (denominator != 0.0f) {
                sub[j * cols + i] = (sub[j * cols + i] - min) / denominator;
            } else {
                sub[j * cols + i] = 0.0f;
            }
        }
    }

    // print mmt result for small matrices
    if (cols <= 32) {
        pthread_mutex_lock(&console_lock);
        printf("\n--- Slave %d Local MMT (%d x %d) ---\n", t_id, rows, cols);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                printf("%.4f\t", sub[r * cols + c]);
            }
            printf("\n");
        }
        printf("------------------------------------\n");
        pthread_mutex_unlock(&console_lock);
    }

    free(sub);
}

// struct to pass args to send threads
typedef struct {
    int *sub;
    int rows;
    int n;
    int start_row;
    int first_slave;
    int num_slaves;
    int t_id;
    char ip[64];
    int port;
    int success;
    char ack[4];
} TArgs;

// thread worker: connect to child slave, send metadata + submatrix, receive ack
void *worker_func(void *args) {
    TArgs *my_args = (TArgs *)args;
    my_args->success = 0;
    memset(my_args->ack, 0, 4);

    int sub_bytes = my_args->rows * my_args->n * (int)sizeof(int);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return NULL;

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((unsigned short)my_args->port);
    inet_pton(AF_INET, my_args->ip, &saddr.sin_addr);

    // retry connection until child slave is ready
    int connected = 0;
    for (int retry = 0; retry < 30; retry++) {
        if (connect(sock, (struct sockaddr *)&saddr, sizeof(saddr)) == 0) {
            connected = 1;
            break;
        }
        Sleep(500);
    }

    if (!connected) {
        closesocket(sock);
        return NULL;
    }

    // send metadata: n, rows, start_row, first_slave, num_slaves
    send_all(sock, (char *)&my_args->n, sizeof(int));
    send_all(sock, (char *)&my_args->rows, sizeof(int));
    send_all(sock, (char *)&my_args->start_row, sizeof(int));
    send_all(sock, (char *)&my_args->first_slave, sizeof(int));
    send_all(sock, (char *)&my_args->num_slaves, sizeof(int));
    send_all(sock, (char *)my_args->sub, sub_bytes);

    // receive ack from child (ack arrives after child's subtree is complete)
    if (recv_all(sock, my_args->ack, 3) >= 0) {
        my_args->success = 1;
    }

    closesocket(sock);
    return NULL;
}

// run master: create matrix, distribute via tree-based O(log t) broadcast
int run_master(int n, int p, char *config_file, char *input_file) {
    // read config file for slave ips and ports
    FILE *cfg = fopen(config_file, "r");
    if (!cfg) { perror("Error opening config file"); return 1; }

    char slave_ips[64][64];
    int slave_ports[64];
    int t = 0;
    char line[256];

    while (fgets(line, 256, cfg)) {
        char role[16], ip[64];
        int port;
        if (sscanf(line, "%15s %63s %d", role, ip, &port) != 3) continue;
        if (strcmp(role, "slave") == 0 && t < 64) {
            strncpy(slave_ips[t], ip, 63);
            slave_ips[t][63] = '\0';
            slave_ports[t] = port;
            t++;
        }
    }
    fclose(cfg);

    if (t == 0) {
        printf("Error: no slaves found in config file.\n");
        return 1;
    }

    int file_mode = (input_file != NULL);
    printf("[Master] n=%d, slaves=%d, mode=%s\n",
           n, t, file_mode ? "file" : "random");

    // create or read the matrix
    int *M = NULL;

    if (file_mode) {
        FILE *fp = fopen(input_file, "r");
        if (!fp) { perror("Error opening input file"); return 1; }

        int file_n;
        if (fscanf(fp, "%d", &file_n) != 1 || file_n <= 0) {
            printf("Error: Could not read valid n from file.\n");
            fclose(fp); return 1;
        }
        if (file_n != n) {
            printf("[Master] file n=%d overrides arg n=%d\n", file_n, n);
            n = file_n;
        }

        M = (int *)malloc((size_t)n * n * sizeof(int));
        if (M == NULL) {
            printf("Memory allocation failed.\n");
            fclose(fp); return 1;
        }

        for (int k = 0; k < n * n; k++) {
            if (fscanf(fp, "%d", &M[k]) != 1) {
                printf("Error: File does not contain enough data.\n");
                free(M); fclose(fp); return 1;
            }
            int ch = fgetc(fp);
            if (ch != ',') {
                ungetc(ch, fp);
            }
        }
        fclose(fp);
    } else {
        M = (int *)malloc((size_t)n * n * sizeof(int));
        if (M == NULL) {
            printf("Memory allocation failed.\n");
            return 1;
        }

        srand(time(NULL));

        // fill matrix with random values between 1 and 100
        for (int k = 0; k < n * n; k++) {
            int r = (rand() % 100) + 1;
            M[k] = r;
        }
    }

    if (file_mode || n <= 32) print_matrix(M, n);

    // print all final slave assignments for verification (same as before)
    int row_chunk = n / t;
    int rem = n % t;
    int curr_r = 0;
    for (int i = 0; i < t; i++) {
        int assigned_r = row_chunk + (i < rem ? 1 : 0);
        printf("\n[Master] Slave %d (%s:%d) -> %d rows (rows %d to %d)\n",
               i + 1, slave_ips[i], slave_ports[i],
               assigned_r, curr_r, curr_r + assigned_r - 1);
        if (file_mode || n <= 32) {
            print_submatrix(&M[(size_t)curr_r * n], assigned_r, n, i + 1, curr_r);
        }
        curr_r += assigned_r;
    }

    // compute tree children: iteratively split [first, first+count) range
    // keep left half, send right half to child
    TArgs *t_args = malloc(32 * sizeof(TArgs));
    int num_children = 0;

    int rem_first = 0;      // first slave index (0-based) in remaining range
    int rem_count = t;      // number of slaves in remaining range
    int rem_start_row = 0;  // global start row of remaining data
    int rem_rows = n;       // rows in remaining data

    while (rem_count > 0) {
        if (rem_count == 1) {
            // last slave: send all remaining rows
            int child_slave = rem_first;
            t_args[num_children].sub = &M[(size_t)rem_start_row * n];
            t_args[num_children].rows = rem_rows;
            t_args[num_children].n = n;
            t_args[num_children].start_row = rem_start_row;
            t_args[num_children].first_slave = child_slave;
            t_args[num_children].num_slaves = 1;
            t_args[num_children].t_id = child_slave + 1;
            strncpy(t_args[num_children].ip, slave_ips[child_slave], 63);
            t_args[num_children].ip[63] = '\0';
            t_args[num_children].port = slave_ports[child_slave];
            num_children++;
            break;
        }

        int left_count = rem_count / 2;
        int right_count = rem_count - left_count;
        int right_first = rem_first + left_count;
        int left_rows = compute_rows_for_range(n, t, rem_first, left_count);
        int right_rows = rem_rows - left_rows;

        // send right half to the first slave in the right range
        int child_slave = right_first;
        t_args[num_children].sub = &M[(size_t)(rem_start_row + left_rows) * n];
        t_args[num_children].rows = right_rows;
        t_args[num_children].n = n;
        t_args[num_children].start_row = rem_start_row + left_rows;
        t_args[num_children].first_slave = right_first;
        t_args[num_children].num_slaves = right_count;
        t_args[num_children].t_id = child_slave + 1;
        strncpy(t_args[num_children].ip, slave_ips[child_slave], 63);
        t_args[num_children].ip[63] = '\0';
        t_args[num_children].port = slave_ports[child_slave];
        num_children++;

        // master keeps left half
        rem_count = left_count;
        rem_rows = left_rows;
    }

    printf("\n[Master] Tree distribution: %d direct children (O(log %d) = %d steps)\n",
           num_children, t, num_children);
    for (int i = 0; i < num_children; i++) {
        printf("[Master] -> Slave %d: %d rows, covers %d slave(s)\n",
               t_args[i].t_id, t_args[i].rows, t_args[i].num_slaves);
    }

    // create ack listener on master port for direct slave acks
    SOCKET ack_srv = socket(AF_INET, SOCK_STREAM, 0);
    if (ack_srv == INVALID_SOCKET) {
        printf("Error: ack listener socket failed.\n");
        free(t_args); free(M); return 1;
    }
    int ack_opt = 1;
    setsockopt(ack_srv, SOL_SOCKET, SO_REUSEADDR, (char *)&ack_opt, sizeof(ack_opt));
    struct sockaddr_in ack_addr;
    memset(&ack_addr, 0, sizeof(ack_addr));
    ack_addr.sin_family = AF_INET;
    ack_addr.sin_addr.s_addr = INADDR_ANY;
    ack_addr.sin_port = htons((unsigned short)p);
    if (bind(ack_srv, (struct sockaddr *)&ack_addr, sizeof(ack_addr)) == SOCKET_ERROR) {
        printf("Error: ack listener bind failed on port %d\n", p);
        closesocket(ack_srv); free(t_args); free(M); return 1;
    }
    if (listen(ack_srv, t) == SOCKET_ERROR) {
        printf("Error: ack listener listen failed.\n");
        closesocket(ack_srv); free(t_args); free(M); return 1;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // launch all child send threads concurrently (tree distribution)
    pthread_t *worker_threads = malloc(num_children * sizeof(pthread_t));
    for (int i = 0; i < num_children; i++) {
        t_args[i].success = 0;
        memset(t_args[i].ack, 0, 4);
        if (pthread_create(&worker_threads[i], NULL, worker_func, &t_args[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // collect direct acks from all t slaves
    printf("\n");
    int acks_received = 0;
    for (int i = 0; i < t; i++) {
        SOCKET ack_csock = accept(ack_srv, NULL, NULL);
        if (ack_csock != INVALID_SOCKET) {
            int sid;
            char ack_buf[4] = {0};
            if (recv_all(ack_csock, (char *)&sid, sizeof(int)) >= 0 &&
                recv_all(ack_csock, ack_buf, 3) >= 0) {
                printf("[Master] Received '%s' from Slave %d\n", ack_buf, sid);
                acks_received++;
            }
            closesocket(ack_csock);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    closesocket(ack_srv);

    // wait for tree threads to finish (cleanup)
    for (int i = 0; i < num_children; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    printf("[Master] Acknowledgements: %d/%d\n", acks_received, t);

    double time_elapsed = (end.tv_sec - start.tv_sec) +
                          (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("time elapsed: %.6f seconds\n", time_elapsed);

    free(worker_threads);
    free(t_args);
    free(M);
    return 0;
}

// run slave: listen, receive submatrix, forward to children if needed, compute mmt, send ack
int run_slave(int n, int p, char *config_file, int core_id) {
    // pin to core if specified (reused from lab03)
    if (core_id >= 0) {
        #ifdef _WIN32
            DWORD_PTR mask = (DWORD_PTR)1 << core_id;
            if (SetProcessAffinityMask(GetCurrentProcess(), mask)) {
                printf("[Slave] Pinned to core %d\n", core_id);
            }
        #else
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);
            if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0) {
                printf("[Slave] Pinned to core %d\n", core_id);
            }
        #endif
    }

    // read full config: need all slave ips/ports for tree forwarding + master for direct ack
    FILE *cfg = fopen(config_file, "r");
    if (!cfg) { perror("Error opening config file"); return 1; }

    char slave_ips[64][64];
    int slave_ports[64];
    char master_ip[64] = "";
    int master_port = 0;
    int t = 0;
    int slave_id = 1;
    char line[256];

    while (fgets(line, 256, cfg)) {
        char role[16], ip[64];
        int port;
        if (sscanf(line, "%15s %63s %d", role, ip, &port) != 3) continue;
        if (strcmp(role, "master") == 0) {
            strncpy(master_ip, ip, 63);
            master_ip[63] = '\0';
            master_port = port;
        } else if (strcmp(role, "slave") == 0 && t < 64) {
            strncpy(slave_ips[t], ip, 63);
            slave_ips[t][63] = '\0';
            slave_ports[t] = port;
            if (port == p) slave_id = t + 1;
            t++;
        }
    }
    fclose(cfg);

    printf("[Slave %d] Listening on port %d...\n", slave_id, p);

    // create server socket
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) {
        printf("Error: socket creation failed.\n");
        return 1;
    }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)p);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Error: bind failed on port %d\n", p);
        closesocket(srv); return 1;
    }
    if (listen(srv, 1) == SOCKET_ERROR) {
        printf("Error: listen failed on port %d\n", p);
        closesocket(srv); return 1;
    }

    // wait for parent (master or another slave) to connect
    struct sockaddr_in parent_addr;
    socklen_t addr_len = sizeof(parent_addr);
    SOCKET csock = accept(srv, (struct sockaddr *)&parent_addr, &addr_len);
    if (csock == INVALID_SOCKET) {
        printf("Error: accept failed.\n");
        closesocket(srv); return 1;
    }

    struct timespec start, end;

    // time starts when parent initiates connection
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("[Slave %d] Connected by parent.\n", slave_id);

    // receive metadata: n, rows, start_row, first_slave, num_slaves
    int recv_n, rows, start_row, first_slave, num_slaves;
    if (recv_all(csock, (char *)&recv_n, sizeof(int)) < 0 ||
        recv_all(csock, (char *)&rows, sizeof(int)) < 0 ||
        recv_all(csock, (char *)&start_row, sizeof(int)) < 0 ||
        recv_all(csock, (char *)&first_slave, sizeof(int)) < 0 ||
        recv_all(csock, (char *)&num_slaves, sizeof(int)) < 0) {
        printf("Error: failed to receive metadata.\n");
        closesocket(csock); closesocket(srv); return 1;
    }

    printf("[Slave %d] Receiving %d rows x %d cols (rows %d to %d), covers %d slave(s)\n",
           slave_id, rows, recv_n, start_row, start_row + rows - 1, num_slaves);

    // receive submatrix data
    int sub_bytes = rows * recv_n * (int)sizeof(int);
    int *sub = (int *)malloc((size_t)sub_bytes);
    if (sub == NULL) {
        printf("Memory allocation failed.\n");
        closesocket(csock); closesocket(srv); return 1;
    }

    if (recv_all(csock, (char *)sub, sub_bytes) < 0) {
        printf("Error: failed to receive submatrix data.\n");
        free(sub); closesocket(csock); closesocket(srv); return 1;
    }

    // forward to children if this slave covers more than 1 slave
    int num_children = 0;
    TArgs *child_args = NULL;
    pthread_t *child_threads = NULL;

    if (num_slaves > 1) {
        child_args = malloc(32 * sizeof(TArgs));
        child_threads = malloc(32 * sizeof(pthread_t));

        int rem_first = first_slave;
        int rem_count = num_slaves;
        int rem_start = start_row;
        int rem_rows = rows;
        int data_offset = 0;  // row offset within sub array

        while (rem_count > 1) {
            int left_count = rem_count / 2;
            int right_count = rem_count - left_count;
            int right_first = rem_first + left_count;
            int left_rows = compute_rows_for_range(recv_n, t, rem_first, left_count);
            int right_rows = rem_rows - left_rows;

            // send right portion to child
            int child_slave = right_first;
            child_args[num_children].sub = sub + (size_t)(data_offset + left_rows) * recv_n;
            child_args[num_children].rows = right_rows;
            child_args[num_children].n = recv_n;
            child_args[num_children].start_row = rem_start + left_rows;
            child_args[num_children].first_slave = right_first;
            child_args[num_children].num_slaves = right_count;
            child_args[num_children].t_id = child_slave + 1;
            strncpy(child_args[num_children].ip, slave_ips[child_slave], 63);
            child_args[num_children].ip[63] = '\0';
            child_args[num_children].port = slave_ports[child_slave];
            child_args[num_children].success = 0;
            memset(child_args[num_children].ack, 0, 4);

            printf("[Slave %d] Forwarding %d rows to Slave %d (covers %d slave(s))\n",
                   slave_id, right_rows, child_slave + 1, right_count);

            if (pthread_create(&child_threads[num_children], NULL, worker_func,
                               &child_args[num_children]) != 0) {
                perror("Failed to create thread");
            }
            num_children++;

            // keep left portion
            rem_count = left_count;
            rem_rows = left_rows;
            // data_offset stays same (left portion is at current offset)
            // rem_first stays same
        }

        // after loop: my final data is sub[data_offset * recv_n], size rem_rows
        rows = rem_rows;
        start_row = rem_start;
    }

    // print received submatrix for verification (my final portion)
    if (recv_n <= 32) {
        print_submatrix(sub, rows, recv_n, slave_id, start_row);
    }

    // compute local mmt on my final submatrix (prep for lab05)
    mmt(sub, rows, recv_n, slave_id);

    // wait for all children to complete before sending ack
    for (int i = 0; i < num_children; i++) {
        pthread_join(child_threads[i], NULL);
        if (child_args[i].success) {
            printf("[Slave %d] Received ack from Slave %d\n",
                   slave_id, child_args[i].t_id);
        }
    }

    // send tree ack to parent (keeps data connection clean)
    send_all(csock, "ack", 3);

    // send direct ack to master (as required by spec)
    SOCKET ack_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ack_sock != INVALID_SOCKET) {
        struct sockaddr_in m_addr;
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons((unsigned short)master_port);
        inet_pton(AF_INET, master_ip, &m_addr.sin_addr);

        int ack_ok = 0;
        for (int retry = 0; retry < 30; retry++) {
            if (connect(ack_sock, (struct sockaddr *)&m_addr, sizeof(m_addr)) == 0) {
                ack_ok = 1;
                break;
            }
            Sleep(500);
        }
        if (ack_ok) {
            send_all(ack_sock, (char *)&slave_id, sizeof(int));
            send_all(ack_sock, "ack", 3);
            printf("[Slave %d] Sent 'ack' to master.\n", slave_id);
        }
        closesocket(ack_sock);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_elapsed = (end.tv_sec - start.tv_sec) +
                          (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("time elapsed: %.6f seconds\n", time_elapsed);

    if (child_args) free(child_args);
    if (child_threads) free(child_threads);
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

    if (n <= 0) {
        printf("Error: Please provide a positive integer n.\n");
        return 1;
    }

    // initialize winsock
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("Error: WSAStartup failed.\n");
            return 1;
        }
    #endif

    if (s == 0) {
        // master mode
        char *input_file = NULL;
        if (argc >= 6) input_file = argv[5];

        run_master(n, p, config_file, input_file);

    } else if (s == 1) {
        // slave mode
        int core_id = -1;
        if (argc >= 6) core_id = atoi(argv[5]);

        run_slave(n, p, config_file, core_id);

    } else {
        printf("Error: s must be 0 (master) or 1 (slave).\n");
    }

    // cleanup winsock
    #ifdef _WIN32
        WSACleanup();
    #endif

    return 0;
}
