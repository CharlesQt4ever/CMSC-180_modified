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

// modified mmt to accept the full matrix but only process assigned columns
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

// left untouched for you to edit
void print_submatrix(float *subX, int m, int num_cols, int t_id) {
    pthread_mutex_lock(&console_lock);
    printf("\n--- Thread %d Submatrix (%d x %d) ---\n", t_id, m, num_cols);
    for (int r_idx = 0; r_idx < m; r_idx++) {
        for (int c_idx = 0; c_idx < num_cols; c_idx++) {
            printf("%.4f\t", subX[c_idx * m + r_idx]);
        }
        printf("\n");
    }
    printf("------------------------------------\n");
    pthread_mutex_unlock(&console_lock);
}

// struct to pass args to threads
typedef struct {
    float *X;
    int n;
    int start_col;
    int end_col;
    int t_id;
} TArgs;

void *worker_func(void *args) {
    TArgs *my_args = (TArgs *)args;

    // print_submatrix call commented out so you can handle printing later
    // print_submatrix(my_args->X, my_args->n, my_args->end_col - my_args->start_col, my_args->t_id);

    mmt(my_args->X, my_args->n, my_args->start_col, my_args->end_col);
    
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

        // populate matrix exactly like lab01.c
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

        // populate matrix exactly like lab01.c
        for (int k = 0; k < n * n; k++) {
            int r = (rand() % 100) + 1; 
            X[k] = (float)r;
        }
    }
    
    // untouched print loop
    // for(int i = 0; i < n; i++) {
    //     for(int j = 0; j < n; j++) {
    //         printf("%.4f\t", X[i * n + j]);
    //     }
    //     printf("\n");
    // }

    // thread management arrays
    pthread_t *worker_threads = malloc(t * sizeof(pthread_t));
    TArgs *t_args = malloc(t * sizeof(TArgs));
    
    int col_chunk = n / t;
    int rem = n % t;
    int curr_c = 0;

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

    // timer stops exactly here, before the printing happens
    clock_gettime(CLOCK_MONOTONIC, &end);

    // untouched print loop
    // for(int i = 0; i < n; i++) {
    //     for(int j = 0; j < n; j++) {
    //         printf("%.4f\t", X[i * n + j]);
    //     }
    //     printf("\n");
    // }

    double time_elapsed = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("time elapsed: %.6f seconds\n", time_elapsed);

    free(worker_threads);
    free(t_args);
    free(X);
    return 0;
}