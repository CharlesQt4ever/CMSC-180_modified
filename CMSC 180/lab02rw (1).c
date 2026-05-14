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

// lock for clean printing
pthread_mutex_t console_lock = PTHREAD_MUTEX_INITIALIZER;

// struct to pass args to threads
typedef struct {
    float *X;
    int m;
    int start_row;
    int end_row;
    float *local_min;
    float *local_max;
    float *global_min;
    float *global_max;
    int t_id;
} TArgs;

void print_submatrix(float *X, int m, int start_row, int end_row, int t_id) {
    pthread_mutex_lock(&console_lock);
    printf("\n--- Thread %d Submatrix (Rows %d to %d) ---\n", t_id, start_row, end_row - 1);
    for (int r_idx = start_row; r_idx < end_row; r_idx++) {
        for (int c_idx = 0; c_idx < m; c_idx++) {
            printf("%.4f\t", X[r_idx * m + c_idx]);
        }
        printf("\n");
    }
    printf("------------------------------------\n");
    pthread_mutex_unlock(&console_lock);
}

// phase 1: find local extremes
void *worker_func_phase1(void *args) {
    TArgs *my_args = (TArgs *)args;
    int m = my_args->m;

    for (int c_idx = 0; c_idx < m; c_idx++) {
        my_args->local_min[c_idx] = FLT_MAX;
        my_args->local_max[c_idx] = -FLT_MAX;
    }

    for (int r_idx = my_args->start_row; r_idx < my_args->end_row; r_idx++) {
        for (int c_idx = 0; c_idx < m; c_idx++) {
            float val = my_args->X[r_idx * m + c_idx];
            if (val < my_args->local_min[c_idx]) my_args->local_min[c_idx] = val;
            if (val > my_args->local_max[c_idx]) my_args->local_max[c_idx] = val;
        }
    }
    
    return NULL;
}

// phase 2: apply mmt formula
void *worker_func_phase2(void *args) {
    TArgs *my_args = (TArgs *)args;
    int m = my_args->m;

    print_submatrix(my_args->X, my_args->m, my_args->start_row, my_args->end_row, my_args->t_id);

    for (int r_idx = my_args->start_row; r_idx < my_args->end_row; r_idx++) {
        for (int c_idx = 0; c_idx < m; c_idx++) {
            float min = my_args->global_min[c_idx];
            float max = my_args->global_max[c_idx];
            float denominator = max - min;

            if (denominator != 0.0f) {
                my_args->X[r_idx * m + c_idx] = (my_args->X[r_idx * m + c_idx] - min) / denominator;
            } else {
                my_args->X[r_idx * m + c_idx] = 0.0f;
            }
        }
    }
    
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

        // populate matrix (row-major)
        for (int r_idx = 0; r_idx < n; r_idx++) {
            for (int c_idx = 0; c_idx < n; c_idx++) {
                if (fscanf(fp, "%f", &X[r_idx * n + c_idx]) != 1) {
                    printf("Error: File does not contain enough data.\n");
                    free(X); fclose(fp); return 1; 
                }
                int ch = fgetc(fp);
                if (ch != ',') {
                    ungetc(ch, fp);
                }
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

        for (int r_idx = 0; r_idx < n; r_idx++) {
            for (int c_idx = 0; c_idx < n; c_idx++) {
                int r = (rand() % 100) + 1; 
                X[r_idx * n + c_idx] = (float)r;
            }
        }
    }

    // thread management arrays
    pthread_t *worker_threads = malloc(t * sizeof(pthread_t));
    TArgs *t_args = malloc(t * sizeof(TArgs));
    
    float *global_min = malloc(n * sizeof(float));
    float *global_max = malloc(n * sizeof(float));

    int row_chunk = n / t;
    int rem = n % t;
    int curr_r = 0;

    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    // execute phase 1
    for (int i = 0; i < t; i++) {
        int assigned_r = (i == t - 1) ? (row_chunk + rem) : row_chunk;
        
        t_args[i].X = X;
        t_args[i].m = n;
        t_args[i].start_row = curr_r;
        t_args[i].end_row = curr_r + assigned_r;
        t_args[i].t_id = i + 1;
        
        t_args[i].local_min = malloc(n * sizeof(float));
        t_args[i].local_max = malloc(n * sizeof(float));

        if (pthread_create(&worker_threads[i], NULL, worker_func_phase1, &t_args[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
        
        curr_r += assigned_r;
    }

    // wait for phase 1 to finish
    for (int i = 0; i < t; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    // main thread gathers global extremes
    for (int c_idx = 0; c_idx < n; c_idx++) {
        global_min[c_idx] = FLT_MAX;
        global_max[c_idx] = -FLT_MAX;
        for (int i = 0; i < t; i++) {
            if (t_args[i].local_min[c_idx] < global_min[c_idx]) global_min[c_idx] = t_args[i].local_min[c_idx];
            if (t_args[i].local_max[c_idx] > global_max[c_idx]) global_max[c_idx] = t_args[i].local_max[c_idx];
        }
    }

    // execute phase 2
    for (int i = 0; i < t; i++) {
        t_args[i].global_min = global_min;
        t_args[i].global_max = global_max;

        if (pthread_create(&worker_threads[i], NULL, worker_func_phase2, &t_args[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // wait for phase 2 to finish
    for (int i = 0; i < t; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double time_elapsed = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("time elapsed: %.6f seconds\n", time_elapsed);

    // cleanup
    for (int i = 0; i < t; i++) {
        free(t_args[i].local_min);
        free(t_args[i].local_max);
    }
    free(worker_threads);
    free(t_args);
    free(global_min);
    free(global_max);
    free(X);

    return 0;
}