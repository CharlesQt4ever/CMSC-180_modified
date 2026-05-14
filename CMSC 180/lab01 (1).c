/*
Charles Andrei P. De los Reyes
2023-15797
B-3L
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>

// performs min-max transformation on columns
void mmt(float *X, int m, int n) {
    int i, j;
    
    // iterate through each column
    for (i = 0; i < n; i++) {
        
        // initialize min and max trackers
        float min = FLT_MAX;
        float max = -FLT_MAX;

        // find min and max values in the current column
        for (j = 0; j < m; j++) {
            float val = X[j * n + i]; 
            if (val < min) min = val;
            if (val > max) max = val;
        }

        // calculate the range
        float denominator = max - min;

        // update elements using the normalization formula
        for (j = 0; j < m; j++) {
            
            // check to avoid division by zero
            if (denominator != 0.0f) {
                X[j * n + i] = (X[j * n + i] - min) / denominator;
            } else {
                X[j * n + i] = 0.0f;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int n;
    float *X;
    
    // check if a file path argument was provided
    if (argc > 1) {
        
        // open file for reading
        FILE *fp = fopen(argv[1], "r");
        if (!fp) { perror("Error opening file"); return 1; }

        // read the matrix dimension n
        if (fscanf(fp, "%d", &n) != 1) { 
            printf("Error: Could not read n from file.\n");
            fclose(fp); return 1; 
        }

        // allocate memory for the n*n matrix
        X = (float *)malloc((size_t)n * n * sizeof(float));
        if (X == NULL) {
            printf("Memory allocation failed.\n");
            fclose(fp); return 1; 
        }

        // load data from file into the array
        for (int k = 0; k < n * n; k++) {
            if (fscanf(fp, "%f", &X[k]) != 1) {
                printf("Error: File does not contain enough data.\n");
                free(X); fclose(fp); return 1; 
            }
        }
        fclose(fp);

    } else {
        // no file provided, get n from user input
        if (scanf("%d", &n) != 1 || n <= 0) {
            printf("Error: Please provide a positive integer n.\n");
            return 1;
        }

        X = (float *)malloc((size_t)n * n * sizeof(float));
        if (X == NULL) {
            printf("Memory allocation failed.\n");
            return 1;
        }

        // seed the random number generator
        srand(time(NULL));

        // fill matrix with random values between 1 and 100
        for (int k = 0; k < n * n; k++) {
            int r = (rand() % 100) + 1; 
            X[k] = (float)r;
        }
    }

    struct timespec start, end;

    // start high-precision timer
    clock_gettime(CLOCK_MONOTONIC, &start);

    // execute the normalization logic
    mmt(X, n, n); 

    // stop high-precision timer
    clock_gettime(CLOCK_MONOTONIC, &end);

    // calculate total duration in seconds
    double time_elapsed = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("time elapsed: %.6f seconds\n", time_elapsed);

    free(X);
    return 0;
}