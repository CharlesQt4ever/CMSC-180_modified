/*
Charles Andrei P. De los Reyes
2023-15797
B-3L
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>
#include <pthread.h>

// include windows api for thread affinity
#ifdef _WIN32
#include <windows.h>
#endif

// lock for clean printing
pthread_mutex_t console_lock = PTHREAD_MUTEX_INITIALIZER;

void mmt(float *X, int n, int start_col, int end_col) {
    for (int i = start_col; i < end_col; i++) {
        float min = FLT_MAX;
        float max = -FLT_MAX;

        // find min and max for column i
        for (int j = 0; j < n; j++) {
            float val = X[j * n + i]; 
            if (val < min) min = val;
            if (val > max) max = val;
        }

        float denominator = max - min;

        // apply transformation for column i
        for (int j = 0; j < n; j++) {
            if (denominator != 0.0f) {
                X[j * n + i] = (X[j * n + i] - min) / denominator;
            } else {
                X[j * n + i] = 0.0f;
            }
        }
    }
}

// print submatrix on assigned threads
void print_submatrix(float *X, int n, int start_col, int end_col, int t_id) {
    pthread_mutex_lock(&console_lock);
    int num_cols = end_col - start_col;
    printf("\n--- Thread %d Submatrix (%d x %d) ---\n", t_id, n, num_cols);
    for (int r = 0; r < n; r++) {
        for (int c = start_col; c < end_col; c++) {
            printf("%.4f\t", X[r * n + c]);
        }
        printf("\n");
    }
    printf("------------------------------------\n");
    pthread_mutex_unlock(&console_lock);
}

// struct to pass args to threads with core id
typedef struct {
    float *X;
    int n;
    int start_col;
    int end_col;
    int t_id;
    int core_id; 
} TArgs;

void *worker_func(void *args) {
    TArgs *my_args = (TArgs *)args;

    // bitmask for specific core
    #ifdef _WIN32
        DWORD_PTR mask = (DWORD_PTR)1 << my_args->core_id;
        SetThreadAffinityMask(GetCurrentThread(), mask);
    #endif

    // print_submatrix call commented out to handle printing later
    //print_submatrix(my_args->X, my_args->n, my_args->start_col, my_args->end_col, my_args->t_id);

    mmt(my_args->X, my_args->n, my_args->start_col, my_args->end_col);
    
    // print_submatrix call commented out
    //print_submatrix(my_args->X, my_args->n, my_args->start_col, my_args->end_col, my_args->t_id);

    return NULL;
}

int main(int argc, char *argv[]) {
    int n = 0, t = 1;
    float *X = NULL;
    char *input_file = NULL;

    if (argc == 2) {
        input_file = argv[1];
    } else if (argc == 3) {
        n = atoi(argv[1]);
        t = atoi(argv[2]);
    } else {
        printf("Usage for Random Mode: %s <n> <t>\n", argv[0]);
        printf("Usage for File Mode:   %s <input_file.txt>\n", argv[0]);
        return 1;
    }

    if (input_file != NULL) {
        FILE *fp = fopen(input_file, "r");
        if (!fp) { perror("Error opening file"); return 1; }

        if (fscanf(fp, "%d", &n) != 1 || n <= 0) { 
            printf("Error: Could not read valid n from file.\n");
            fclose(fp); return 1; 
        }

        if (fscanf(fp, "%d", &t) != 1 || t <= 0) { 
            printf("Error: Could not read valid t from file.\n");
            fclose(fp); return 1; 
        }

        X = (float *)malloc((size_t)n * n * sizeof(float));
        if (X == NULL) {
            printf("Memory allocation failed.\n");
            fclose(fp); return 1; 
        }

        for (int k = 0; k < n * n; k++) {
            if (fscanf(fp, "%f", &X[k]) != 1) {
                printf("Error: File does not contain enough data.\n");
                free(X); fclose(fp); return 1; 
            }
            int ch = fgetc(fp);
            if (ch != ',') {
                ungetc(ch, fp);
            }
        }
        fclose(fp);
    } else {
        if (n <= 0 || t <= 0) {
            printf("Error: Please provide positive integers for n and t.\n");
            return 1;
        }

        X = (float *)malloc((size_t)n * n * sizeof(float));
        if (X == NULL) {
            printf("Memory allocation failed.\n");
            return 1;
        }

        srand(time(NULL));

        for (int k = 0; k < n * n; k++) {
            int r = (rand() % 100) + 1; 
            X[k] = (float)r;
        }
    }
    
    // thread management arrays
    pthread_t *worker_threads = malloc(t * sizeof(pthread_t));
    TArgs *t_args = malloc(t * sizeof(TArgs));
    
    int col_chunk = n / t;
    int rem = n % t;
    int curr_c = 0;

    // detect system cores
    int num_logical_cores = 1;
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        num_logical_cores = sysinfo.dwNumberOfProcessors;
    #endif

    // reserve core 0 for the os
    int available_cores = (num_logical_cores > 1) ? (num_logical_cores - 1) : 1;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // assign chunks of columns to each thread
    for (int i = 0; i < t; i++) {
        int assigned_c = col_chunk + (i < rem ? 1 : 0);
        
        t_args[i].X = X;
        t_args[i].n = n;
        t_args[i].start_col = curr_c;
        t_args[i].end_col = curr_c + assigned_c;
        t_args[i].t_id = i + 1;
        
        // assign to cores 1 through reserved limit
        t_args[i].core_id = (i % available_cores) + 1; 

        if (pthread_create(&worker_threads[i], NULL, worker_func, &t_args[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
        
        curr_c += assigned_c;
    }

    // wait for all threads to finish
    for (int i = 0; i < t; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    // timer stops exactly here, before printing
    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_elapsed = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("time elapsed: %.6f seconds\n", time_elapsed);

    free(worker_threads);
    free(t_args);
    free(X);
    return 0;
}