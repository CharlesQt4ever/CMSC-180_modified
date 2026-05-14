/*
Charles Andrei P. De los Reyes
2023-15797
B-3L

LRP04 - Distributing Parts of a Matrix over Sockets

Usage:
  lab04 <n> <p> <s> <config_file> [input_file | core_id]

    n           matrix dimension (n x n)
    p           port for this instance
    s           0 = master, 1 = slave
    config_file network configuration file
    input_file  (master, optional) read matrix from file
    core_id     (slave, optional) pin process to CPU core
*/

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
  #include <errno.h>
  #define SOCKET int
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR (-1)
  #define closesocket close
  #define Sleep(ms) usleep((ms) * 1000)
#endif

#define MAX_SLAVES      64
#define MAX_LINE        256
#define VERBOSE_MAX_N   32      /* print matrices only when n <= this */
#define CONNECT_RETRIES 30
#define CONNECT_DELAY_MS 500

typedef struct {
    char ip[64];
    int  port;
} NodeInfo;

typedef struct {
    NodeInfo master;
    NodeInfo slaves[MAX_SLAVES];
    int      num_slaves;
} Config;

/* ============================================================
   Networking helpers
   ============================================================ */

static int init_winsock(void) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return -1;
    }
#endif
    return 0;
}

static void cleanup_winsock(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static int send_all(SOCKET sock, const char *buf, int len) {
    int total = 0;
    while (total < len) {
        int sent = send(sock, buf + total, len - total, 0);
        if (sent == SOCKET_ERROR || sent == 0) return -1;
        total += sent;
    }
    return total;
}

static int recv_all(SOCKET sock, char *buf, int len) {
    int total = 0;
    while (total < len) {
        int got = recv(sock, buf + total, len - total, 0);
        if (got <= 0) return -1;
        total += got;
    }
    return total;
}

/* ============================================================
   Config
   ============================================================ */

static int read_config(const char *filename, Config *config) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("Error opening config file"); return -1; }

    config->num_slaves = 0;
    memset(&config->master, 0, sizeof(NodeInfo));

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        char role[16], ip[64];
        int port;
        if (sscanf(line, "%15s %63s %d", role, ip, &port) != 3) continue;
        if (strcmp(role, "master") == 0) {
            strncpy(config->master.ip, ip, 63);
            config->master.ip[63] = '\0';
            config->master.port = port;
        } else if (strcmp(role, "slave") == 0 && config->num_slaves < MAX_SLAVES) {
            strncpy(config->slaves[config->num_slaves].ip, ip, 63);
            config->slaves[config->num_slaves].ip[63] = '\0';
            config->slaves[config->num_slaves].port = port;
            config->num_slaves++;
        }
    }

    fclose(fp);
    return 0;
}

/* ============================================================
   Matrix I/O

   Input file format (non-randomized mode):
     First token: n
     Then n*n integers, whitespace or comma separated.
   ============================================================ */

static int *read_matrix_from_file(const char *filename, int *out_n) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("Error opening input file"); return NULL; }

    int n;
    if (fscanf(fp, "%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Error: invalid n in input file.\n");
        fclose(fp); return NULL;
    }

    int *M = (int *)malloc((size_t)n * n * sizeof(int));
    if (!M) { fclose(fp); return NULL; }

    for (int k = 0; k < n * n; k++) {
        if (fscanf(fp, "%d", &M[k]) != 1) {
            fprintf(stderr, "Error: input file short of data.\n");
            free(M); fclose(fp); return NULL;
        }
        int ch = fgetc(fp);
        if (ch != ',') ungetc(ch, fp);
    }

    fclose(fp);
    *out_n = n;
    return M;
}

static void print_matrix(const int *M, int n) {
    printf("\n--- Full Matrix M (%d x %d) ---\n", n, n);
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) printf("%d\t", M[r * n + c]);
        printf("\n");
    }
    printf("------------------------------------\n");
}

static void print_submatrix(const int *sub, int rows, int cols, int slave_id, int start_row) {
    printf("\n--- Slave %d Submatrix (%d x %d, rows %d to %d) ---\n",
           slave_id, rows, cols, start_row, start_row + rows - 1);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) printf("%d\t", sub[r * cols + c]);
        printf("\n");
    }
    printf("------------------------------------\n");
}

/* ============================================================
   Master send thread (concurrent one-to-many personalized)

   Each thread handles one slave: connect, send, receive ack.
   All threads run in parallel so distribution is NOT O(t).
   ============================================================ */

typedef struct {
    int    slave_idx;       /* 0-based slave index                */
    int   *sub;             /* pointer into master's M            */
    int    rows_i;          /* number of rows for this slave      */
    int    n;               /* matrix column count                */
    int    start_row;       /* starting row in original matrix    */
    char   ip[64];          /* slave IP (copied from config)      */
    int    port;            /* slave port                         */
    int    success;         /* output: 1 if ack received          */
    char   ack_buf[4];      /* output: ack string from slave      */
} MasterSendArgs;

static void *master_send_worker(void *arg) {
    MasterSendArgs *a = (MasterSendArgs *)arg;
    a->success = 0;
    memset(a->ack_buf, 0, 4);

    int sub_bytes = a->rows_i * a->n * (int)sizeof(int);
    int sid       = a->slave_idx + 1;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "[Master][Thread %d] socket() failed\n", sid);
        return NULL;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port   = htons((unsigned short)a->port);
    inet_pton(AF_INET, a->ip, &saddr.sin_addr);

    /* retry connection until slave is ready */
    int connected = 0;
    for (int retry = 0; retry < CONNECT_RETRIES; retry++) {
        if (connect(sock, (struct sockaddr *)&saddr, sizeof(saddr)) == 0) {
            connected = 1;
            break;
        }
        Sleep(CONNECT_DELAY_MS);
    }

    if (!connected) {
        fprintf(stderr, "[Master][Thread %d] FAILED to connect to Slave %d\n", sid, sid);
        closesocket(sock);
        return NULL;
    }

    /* send metadata: n, rows_i, start_row */
    send_all(sock, (char *)&a->n,         sizeof(int));
    send_all(sock, (char *)&a->rows_i,    sizeof(int));
    send_all(sock, (char *)&a->start_row, sizeof(int));

    /* send submatrix data */
    send_all(sock, (char *)a->sub, sub_bytes);

    /* receive acknowledgement */
    if (recv_all(sock, a->ack_buf, 3) >= 0) {
        a->success = 1;
    }

    closesocket(sock);
    return NULL;
}

/* ============================================================
   Master (s = 0)
   ============================================================ */

static int run_master(int n, int p, const char *config_file, const char *input_file) {
    Config config;
    if (read_config(config_file, &config) != 0) return 1;

    int t = config.num_slaves;
    if (t == 0) {
        fprintf(stderr, "Error: no slaves in config file.\n");
        return 1;
    }

    /* file_mode = non-randomized: always print matrices for verification */
    int file_mode = (input_file != NULL);

    printf("[Master] n=%d, port=%d, slaves=%d, mode=%s\n",
           n, p, t, file_mode ? "file (non-randomized)" : "random");

    /* ---- create or read matrix ---- */
    int *M = NULL;
    if (file_mode) {
        printf("[Master] Reading matrix from: %s\n", input_file);
        int file_n;
        M = read_matrix_from_file(input_file, &file_n);
        if (M && file_n != n) {
            printf("[Master] Note: file n=%d overrides arg n=%d\n", file_n, n);
            n = file_n;
        }
    } else {
        printf("[Master] Generating random %dx%d matrix\n", n, n);
        M = (int *)malloc((size_t)n * n * sizeof(int));
        if (!M) { fprintf(stderr, "Memory allocation failed.\n"); return 1; }
        srand((unsigned)time(NULL));
        for (int k = 0; k < n * n; k++) M[k] = (rand() % 100) + 1;
    }
    if (!M) return 1;

    if (file_mode || n <= VERBOSE_MAX_N) print_matrix(M, n);

    /* ---- prepare per-slave thread arguments ---- */
    int base_rows = n / t;
    int rem       = n % t;

    pthread_t      *threads = (pthread_t *)malloc(t * sizeof(pthread_t));
    MasterSendArgs *args    = (MasterSendArgs *)malloc(t * sizeof(MasterSendArgs));
    int            *created = (int *)calloc(t, sizeof(int));

    if (!threads || !args || !created) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(M); return 1;
    }

    int curr_r = 0;
    for (int i = 0; i < t; i++) {
        int rows_i = base_rows + (i < rem ? 1 : 0);

        args[i].slave_idx = i;
        args[i].sub       = &M[(size_t)curr_r * n];
        args[i].rows_i    = rows_i;
        args[i].n         = n;
        args[i].start_row = curr_r;
        strncpy(args[i].ip, config.slaves[i].ip, 63);
        args[i].ip[63] = '\0';
        args[i].port    = config.slaves[i].port;
        args[i].success = 0;
        memset(args[i].ack_buf, 0, 4);

        /* print what will be sent (sequential, before timer) */
        printf("\n[Master] Assigning Slave %d (%s:%d) — %d rows (rows %d to %d)\n",
               i + 1, config.slaves[i].ip, config.slaves[i].port,
               rows_i, curr_r, curr_r + rows_i - 1);
        if (file_mode || n <= VERBOSE_MAX_N)
            print_submatrix(args[i].sub, rows_i, n, i + 1, curr_r);

        curr_r += rows_i;
    }

    /* ---- timed section: concurrent sends ---- */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    printf("\n[Master] Launching %d send threads concurrently...\n", t);
    for (int i = 0; i < t; i++) {
        if (pthread_create(&threads[i], NULL, master_send_worker, &args[i]) == 0) {
            created[i] = 1;
        } else {
            fprintf(stderr, "[Master] Failed to create thread for Slave %d\n", i + 1);
        }
    }

    /* wait for all threads to finish */
    for (int i = 0; i < t; i++) {
        if (created[i]) pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    /* ---- report results ---- */
    printf("\n");
    int ack_count = 0;
    for (int i = 0; i < t; i++) {
        if (args[i].success) {
            printf("[Master] Received '%s' from Slave %d\n", args[i].ack_buf, i + 1);
            ack_count++;
        } else {
            fprintf(stderr, "[Master] FAILED: no ack from Slave %d\n", i + 1);
        }
    }
    printf("[Master] Acknowledgements: %d/%d slaves responded\n", ack_count, t);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("\ntime elapsed: %.6f seconds\n", elapsed);

    free(created);
    free(threads);
    free(args);
    free(M);
    return 0;
}

/* ============================================================
   Slave local MMT computation (prep for Lab 05)

   Computes column-wise MMT on the received submatrix using
   LOCAL min/max only. In a full distributed MMT (Lab 05),
   global min/max would be aggregated across all slaves first.
   ============================================================ */

static void compute_local_mmt(const int *sub_int, int rows, int cols, int slave_id) {
    int total = rows * cols;
    float *sub = (float *)malloc(total * sizeof(float));
    if (!sub) { fprintf(stderr, "[Slave %d] MMT alloc failed\n", slave_id); return; }

    /* convert int matrix to float */
    for (int k = 0; k < total; k++) sub[k] = (float)sub_int[k];

    /* column-wise MMT using local min/max */
    for (int c = 0; c < cols; c++) {
        float min = sub[0 * cols + c];
        float max = min;
        for (int r = 1; r < rows; r++) {
            float val = sub[r * cols + c];
            if (val < min) min = val;
            if (val > max) max = val;
        }
        float denom = max - min;
        for (int r = 0; r < rows; r++) {
            if (denom != 0.0f)
                sub[r * cols + c] = (sub[r * cols + c] - min) / denom;
            else
                sub[r * cols + c] = 0.0f;
        }
    }

    /* print result for small matrices */
    if (cols <= VERBOSE_MAX_N) {
        printf("\n--- Slave %d Local MMT Result (%d x %d) ---\n", slave_id, rows, cols);
        printf("(Note: uses local min/max only, not global)\n");
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) printf("%.4f\t", sub[r * cols + c]);
            printf("\n");
        }
        printf("------------------------------------\n");
    } else {
        printf("[Slave %d] Local MMT computed on %d x %d submatrix\n",
               slave_id, rows, cols);
    }

    free(sub);
}

/* ============================================================
   Slave (s = 1)
   ============================================================ */

static void pin_to_core(int core_id, int slave_id) {
    if (core_id < 0) return;
#ifdef _WIN32
    DWORD_PTR mask = (DWORD_PTR)1 << core_id;
    if (SetProcessAffinityMask(GetCurrentProcess(), mask)) {
        printf("[Slave %d] Pinned to core %d\n", slave_id, core_id);
    } else {
        fprintf(stderr, "[Slave %d] Warning: could not pin to core %d\n",
                slave_id, core_id);
    }
#else
    (void)core_id;
    fprintf(stderr, "[Slave %d] core affinity not implemented on this platform\n",
            slave_id);
#endif
}

static int run_slave(int p, const char *config_file, int core_id) {
    Config config;
    if (read_config(config_file, &config) != 0) return 1;

    /* determine slave_id early from config */
    int slave_id = 0;
    for (int i = 0; i < config.num_slaves; i++) {
        if (config.slaves[i].port == p) { slave_id = i + 1; break; }
    }
    if (slave_id == 0) slave_id = 1;

    printf("[Slave %d] port=%d, master=%s:%d\n",
           slave_id, p, config.master.ip, config.master.port);
    pin_to_core(core_id, slave_id);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) {
        fprintf(stderr, "[Slave %d] socket() failed.\n", slave_id);
        return 1;
    }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((unsigned short)p);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[Slave %d] bind() failed on port %d\n", slave_id, p);
        closesocket(srv); return 1;
    }
    if (listen(srv, 1) == SOCKET_ERROR) {
        fprintf(stderr, "[Slave %d] listen() failed on port %d\n", slave_id, p);
        closesocket(srv); return 1;
    }

    printf("[Slave %d] Listening on port %d, waiting for master...\n", slave_id, p);

    struct sockaddr_in master_addr;
    socklen_t addr_len = sizeof(master_addr);
    SOCKET csock = accept(srv, (struct sockaddr *)&master_addr, &addr_len);
    if (csock == INVALID_SOCKET) {
        fprintf(stderr, "[Slave %d] accept() failed.\n", slave_id);
        closesocket(srv); return 1;
    }

    /* timer starts when master initiates connection */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("[Slave %d] Master connected.\n", slave_id);

    /* receive metadata */
    int recv_n, rows_count, start_row;
    if (recv_all(csock, (char *)&recv_n,     sizeof(int)) < 0 ||
        recv_all(csock, (char *)&rows_count, sizeof(int)) < 0 ||
        recv_all(csock, (char *)&start_row,  sizeof(int)) < 0) {
        fprintf(stderr, "[Slave %d] error receiving metadata.\n", slave_id);
        closesocket(csock); closesocket(srv); return 1;
    }

    printf("[Slave %d] Expecting: %d rows x %d cols (rows %d to %d)\n",
           slave_id, rows_count, recv_n, start_row, start_row + rows_count - 1);

    /* receive submatrix data */
    int sub_bytes = rows_count * recv_n * (int)sizeof(int);
    int *sub = (int *)malloc((size_t)sub_bytes);
    if (!sub) {
        fprintf(stderr, "[Slave %d] memory allocation failed.\n", slave_id);
        closesocket(csock); closesocket(srv); return 1;
    }

    if (recv_all(csock, (char *)sub, sub_bytes) < 0) {
        fprintf(stderr, "[Slave %d] error receiving submatrix data.\n", slave_id);
        free(sub); closesocket(csock); closesocket(srv); return 1;
    }

    printf("[Slave %d] Received %d bytes (%d rows x %d cols)\n",
           slave_id, sub_bytes, rows_count, recv_n);

    /* print received submatrix for verification (grading basis) */
    if (recv_n <= VERBOSE_MAX_N) {
        printf("[Slave %d] Received submatrix:\n", slave_id);
        print_submatrix(sub, rows_count, recv_n, slave_id, start_row);
    }

    /* compute local MMT on received submatrix (prep for Lab 05) */
    compute_local_mmt(sub, rows_count, recv_n, slave_id);

    /* send acknowledgement to master */
    send_all(csock, "ack", 3);
    printf("[Slave %d] Sent 'ack' to master.\n", slave_id);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("\ntime elapsed: %.6f seconds\n", elapsed);

    free(sub);
    closesocket(csock);
    closesocket(srv);
    return 0;
}

/* ============================================================
   Main
   ============================================================ */

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <n> <p> <s> <config_file> [input_file|core_id]\n\n", argv[0]);
        printf("  n           matrix size (n x n)\n");
        printf("  p           port number\n");
        printf("  s           0 = master, 1 = slave\n");
        printf("  config_file path to config file\n");
        printf("  input_file  (master) matrix data file for non-randomized mode\n");
        printf("  core_id     (slave) CPU core to pin to for core-affine mode\n");
        return 1;
    }

    int   n           = atoi(argv[1]);
    int   p           = atoi(argv[2]);
    int   s           = atoi(argv[3]);
    char *config_file = argv[4];
    char *input_file  = NULL;
    int   core_id     = -1;

    if (argc >= 6) {
        if (s == 0) input_file = argv[5];
        else        core_id    = atoi(argv[5]);
    }

    if (n <= 0) {
        fprintf(stderr, "Error: n must be positive.\n");
        return 1;
    }

    if (init_winsock() != 0) return 1;

    int rc;
    if (s == 0)      rc = run_master(n, p, config_file, input_file);
    else if (s == 1) rc = run_slave(p, config_file, core_id);
    else {
        fprintf(stderr, "Error: s must be 0 (master) or 1 (slave).\n");
        rc = 1;
    }

    cleanup_winsock();
    return rc;
}
