#include <stdio.h>
#include <cblas.h>

int main() {
    // Example: C = alpha * A * B + beta * C
    // Dimensions: A (2x3), B (3x2), C (2x2)
    int m = 2, n = 2, k = 3;
    float alpha = 1.0f;
    float beta = 0.0f;

    // Column-major layout (columns stored sequentially)
    float A[6] = {1, 4,   // first column
                  2, 5,   // second column
                  3, 6};  // third column (2x3)

    float B[6] = {7, 9, 11,  // first column
                  8, 10, 12}; // second column (3x2)

    float C[4] = {0, 0,
                  0, 0};  // 2x2

    // Perform C = alpha*A*B + beta*C
    cblas_sgemm(CblasColMajor, CblasNoTrans, CblasNoTrans,
                m, n, k, alpha, A, m, B, k, beta, C, m);

    // Print result
    printf("Result matrix C:\n");
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            printf("%f ", C[i + j*m]); // column-major indexing
        }
        printf("\n");
    }

    return 0;
}

