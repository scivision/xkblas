/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

extern "C" {

    # include <stdio.h>
    # include <stdlib.h>
    # include <stdint.h>
    # include <string.h>
    # include <time.h>
};

// # include <xkblas/flops.h>

# define XKBLAS_NO_DEFAULT_BLAS_ENUM 1

# include "common/blas.h"
# include "common/blas-version.h"
# include "common/time.cc"
# include "common/utils.cc"

//////////////////////////////
//  TARGETED IMPLEMENTATION //
//////////////////////////////

# include <xkblas/xkblas.h>
# include <xkblas/cblas.h>
# include <xkblas/xkblas.hpp>
static xkblas_t * xkblas;

# include "common/check.cc"
static bool SKIP_CHECK = 0;
static bool ALIGN_MATRICES = 0;
static bool REGISTER_MEMORY = 1;
static bool DUMP_GRAPH = 0;
static int MUMPS_AUTOTUNE_FROM = 0;

/////////////////////////
//  PARAMETERS TO TEST //
/////////////////////////

# define N_CBLAS_SIDE 2
static CBLAS_SIDE SIDE[N_CBLAS_SIDE]       = { CblasLeft,    CblasRight };
static const char * SIDE_STR[N_CBLAS_SIDE] = { "Left ", "Right" };

# define N_CBLAS_UPLO 2
static CBLAS_UPLO UPLO[N_CBLAS_UPLO]       = { CblasUpper,   CblasLower };
static const char * UPLO_STR[N_CBLAS_UPLO] = { "Upper", "Lower" };

# define N_CBLAS_DIAG 2
static CBLAS_DIAG DIAG[N_CBLAS_DIAG]       = { CblasNonUnit, CblasUnit  };
static const char * DIAG_STR[N_CBLAS_DIAG]  = { "NonUnit", "Unit   " };

# define N_CBLAS_TRANSPOSE 3
static CBLAS_TRANSPOSE TRANS[N_CBLAS_TRANSPOSE]  = { CblasNoTrans, CblasTrans, CblasConjTrans };
static const char * TRANS_STR[N_CBLAS_TRANSPOSE] = { "N", "T", "H" };

//////////////////////
//  CLI TO RUN-TIME //
//////////////////////

static void
set_tile_size(int ts)
{
    int p = 0;
    xkblas_set_param(ts, p);
}

TYPED
static void
preallocate(
    TYPE * M,
    int m, int n,
    int ld
) {
    for (xkrt::device_global_id_t device_global_id = 0 ; device_global_id < xkblas->runtime.get_ndevices() ; ++device_global_id)
    {
        if (device_global_id == HOST_DEVICE_GLOBAL_ID)
            continue ;

        xkblas->runtime.memory_noncoherent_alloc(
            device_global_id, MATRIX_COLMAJOR, M, ld, m, n, sizeof(TYPE)
        );
    }
}

TYPED
static void
premove(
    TYPE * M,
    int m, int n,
    int ld
) {
    for (xkrt::device_global_id_t device_global_id = 0 ; device_global_id < xkblas->runtime.get_ndevices() ; ++device_global_id)
    {
        if (device_global_id == HOST_DEVICE_GLOBAL_ID)
            continue ;

        xkblas->memory_coherent_async(
            device_global_id, MATRIX_COLMAJOR, M, ld, m, n, sizeof(TYPE)
        );
    }
    xkblas->sync();
}

TYPED
static void
prepare_csr_matrix(
    double density,
    int m,
    int n,
    int * nnz,
    int ** csr_row_offsets,
    int ** csr_col_indices,
    TYPE ** csr_values
) {
    int max_nnz = (int)(m * n * density) + 1;
    size_t csr_values_size = max_nnz * sizeof(TYPE);
    size_t csr_col_indices_size = max_nnz * sizeof(int);
    size_t csr_row_offsets_size = (m + 1) * sizeof(int);
    size_t memsize = csr_values_size + csr_col_indices_size + csr_row_offsets_size;
    const uintptr_t memory = (const uintptr_t) malloc(memsize);

    if (REGISTER_MEMORY)
    {
        xkblas_register_memory_async((void *) memory, memsize);
        xkblas_register_memory_waitall();
    }

    // Temporary storage for maximum nonzeros
    *csr_values         = (TYPE *) (memory + 0);
    *csr_col_indices    = (int *)  (memory + csr_values_size);
    *csr_row_offsets    = (int *)  (memory + csr_values_size + csr_col_indices_size);

    *nnz = 0; // total nonzeros
    (*csr_row_offsets)[0] = 0;

    for (int i = 0; i < m; ++i)
    {
        if (*nnz < max_nnz)
        {
            for (int j = 0; j < n; ++j)
            {
                if ((double)rand() / RAND_MAX < density)
                {
                    if (*nnz >= max_nnz)
                        break ;

                    (*csr_values)[*nnz]      = (rand() % 9) + 1; // random value 1–9
                    (*csr_col_indices)[*nnz] = j;
                    *nnz = *nnz + 1;
                }
            }
        }
        (*csr_row_offsets)[i+1] = *nnz;
    }
}

TYPED
static void
prepare_n_matrices(uintptr_t * matrices, size_t nmats, size_t ld, size_t n)
{
    /* allocate matrices */
    const size_t s = sizeof(TYPE);
    const uintptr_t alignon = s * ld;
    const uintptr_t memsize = nmats * (alignon + alignon/2 + s * ld * n);

    const uintptr_t mem = (const uintptr_t) malloc(memsize);

    if (REGISTER_MEMORY)
    {
        xkblas_register_memory_async((void *) mem, memsize);
        xkblas_register_memory_waitall();
    }
    FILL((TYPE *)mem, memsize / sizeof(TYPE));

    for (int i = 0 ; i < nmats ; ++i)
    {
        uintptr_t M;

        /* force alignment on LD.s */
        if (ALIGN_MATRICES)
        {
            M = mem + (alignon - (mem % alignon)) + i * s * (ld * n);
            assert(M % alignon == 0);
        }
        /* force unaligned (but align on sizeof(type) still) */
        else
        {
            M = mem + (alignon - (mem % alignon)) + i * s * (ld * n) + alignon / 2;
            assert(M % alignon != 0);

            M = M + (s - (M % s));
            assert(M % s == 0);
        }

        matrices[i] = M;
        assert(M + ld*n <= mem + memsize);
    }
}

TYPED
static void
prepare_n_vectors(uintptr_t * vectors, size_t nvecs, size_t n)
{
    prepare_n_matrices<P>(vectors, nvecs, n, 1);
}

TYPED
static int
main_axpy(char ** args)
{
    /* parse arguments */
    int n = atoi(args[0]);
    int ts = atoi(args[1]);
    TYPE alpha = (const TYPE) 0.0;
    FILL(&alpha, 1);

    /* allocate vectors */
    uintptr_t vectors[3];
    # define X     ((TYPE *)vectors[0])
    # define Y     ((TYPE *)vectors[1])
    # define Y2    ((TYPE *)vectors[2])
    prepare_n_vectors<P>(vectors, 3, n);

    // for small vectors
    if (n <= 64)
    {
        alpha = (TYPE) 0.5;
        memset(X, 0, n * sizeof(TYPE));
        memset(Y, 0, n * sizeof(TYPE));
        for (int i = 0 ; i < n ; ++i)
        {
            X[i] = (TYPE) 1.0;
            Y[i] = (TYPE) 0.25;
        }
        memcpy(Y2, Y, n * sizeof(TYPE));
    }

    uint64_t t0 = get_nanotime();
    set_tile_size(ts);
    xkblas->axpy_async<P>(n, &alpha, X, 1, Y, 1);
    xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, Y, n*sizeof(TYPE));
    uint64_t tt = get_nanotime();
    xkblas_sync();
    uint64_t tf = get_nanotime();

    // for small vectors
    if (n <= 64)
    {
        printf("  a . [  x  ] + [  y  ] = [  y  ]");
        for (int i = 0 ; i < n ; ++i)
            printf("%3f . [ %3f ] + [ %3f ] = [ %3f ]\n", alpha, X[i], Y2[i], Y[i]);
    }

    # undef X
    # undef Y
    # undef Y2

    return 0;
}

// GEMM - GEMM //
TYPED
static int
main_gemm_gemm(char ** args)
{
    /* parse arguments */
    int m   = atoi(args[0]);
    int n   = atoi(args[1]);
    int k   = atoi(args[2]);
    int ts1 = atoi(args[3]);
    int ts2 = atoi(args[4]);

    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;

    /* currently only support this */
    printf("Set (m, n, k) = (%d, %d, %d)\n", m, n, k);
    int ld = MAX(MAX(m, n), k);

    /* allocate matrices */
    uintptr_t matrices[5];
    # define A     ((TYPE *)matrices[0])
    # define B     ((TYPE *)matrices[1])
    # define C     ((TYPE *)matrices[2])
    # define CRef  ((TYPE *)matrices[3])
    # define CImpl ((TYPE *)matrices[4])
    prepare_n_matrices<P>(matrices, 5, ld, ld);
    FILL(&alpha, 1);
    FILL(&beta, 1);

    int t1 = 0;
    int t2 = 0;

    for (int t1 = 0 ; t1 < N_CBLAS_TRANSPOSE ; ++t1)
    {
        for (int t2 = 0 ; t2 < N_CBLAS_TRANSPOSE ; ++t2)
        {
            xkblas_memory_invalidate_caches();
            memcpy(CImpl, C, sizeof(TYPE) * (ld * ld));

            /* run on impl */
            printf("Running implementation with (%s, %s)\n", TRANS_STR[t1], TRANS_STR[t2]);
            {
                uint64_t t0 = get_nanotime();
                set_tile_size(ts1);
                xkblas->gemm_async<P>(TRANS[t1], TRANS[t2], m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);

                set_tile_size(ts2);
                xkblas->gemm_async<P>(TRANS[t1], TRANS[t2], m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);

                xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, CImpl, ld, m, n, sizeof(TYPE));

                uint64_t tt = get_nanotime();
                xkblas_sync();
                uint64_t tf = get_nanotime();
                printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);
            }

            if (!SKIP_CHECK)
            {
                /* check correctness */
                memcpy(CRef,  C, sizeof(TYPE) * (ld * ld));
                int r = gemm_cmp<P>(TRANS[t1], TRANS[t2], m, n, k, alpha, A, ld, B, ld, beta, C, CRef, CImpl, ld, 2);
                printf("Result is %12s with trans (%s, %s)\n", (r == 0) ? "CORRECT" : "INCORRECT", TRANS_STR[t1], TRANS_STR[t2]);
            }
        }
    }

    # undef A
    # undef B
    # undef C
    # undef CRef
    # undef CImpl

    return 0;
}

// GEMM //
TYPED
static int
main_gemm(char ** args)
{
    /* parse arguments */
    int m = atoi(args[0]);
    int n = atoi(args[1]);
    int k = atoi(args[2]);
    int ts = atoi(args[3]);
    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;

    /* currently only support this */
    int ld = MAX(MAX(m, n), k) * 2;

    printf("Set (m, n, k) = (%d, %d, %d)\n", m, n, k);

    FILL(&alpha, 1);
    FILL(&beta, 1);

    /* allocate matrices */
    uintptr_t matrices[5];
    # define A     ((TYPE *)matrices[0])
    # define B     ((TYPE *)matrices[1])
    # define C     ((TYPE *)matrices[2])
    # define CRef  ((TYPE *)matrices[3])
    # define CImpl ((TYPE *)matrices[4])
    prepare_n_matrices<P>(matrices, 5, ld, ld);

    // for small matrices
    if (m <= 64)
    {
        alpha = (TYPE) 1.0;
        beta  = (TYPE) 1.0;
        memset(A, 0, ld * ld * sizeof(TYPE));
        memset(B, 0, ld * ld * sizeof(TYPE));
        memset(C, 0, ld * ld * sizeof(TYPE));
        for (int i = 0 ; i < ld ; ++i)
        {
            B[i * ld + i] = 1.0;
            for (int j = 0 ; j < ld ; ++j)
            {
                A[i * ld + j] = (i * m + j) % 9;
                // A[i * ld + j] = (7 * i * j) % 13;
                // A[i * ld + j] = (i + j);
            }
        }
    }

    int t1 = 0;
    int t2 = 0;

    //for (int t1 = 0 ; t1 < N_CBLAS_TRANSPOSE ; ++t1)
    {
        //for (int t2 = 0 ; t2 < N_CBLAS_TRANSPOSE ; ++t2)
        {
            memcpy(CImpl, C, sizeof(TYPE) * (ld * ld));

            printf("Running implementation with (%s, %s)\n", TRANS_STR[t1], TRANS_STR[t2]);
            uint64_t t0 = get_nanotime();
            set_tile_size(ts);
            xkblas->gemm_async<P>(TRANS[t1], TRANS[t2], m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);
            xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, CImpl, ld, m, n, sizeof(TYPE));
            uint64_t tt = get_nanotime();
            xkblas_sync();
            uint64_t tf = get_nanotime();

            printf("Implementation took %lf s. (graph construction took %lf s.)\n", // - %.2lf TFlop/s\n",
                    (tf-t0)/1e9, (tt-t0)/1e9 //, FLOPS_SGEMM(m, n, k) / ((tf-t0)/1e9) / 1e12
            );
            if (!SKIP_CHECK)
            {
                memcpy(CRef,  C, sizeof(TYPE) * (ld * ld));
                int r = gemm_cmp<P>(TRANS[t1], TRANS[t2], m, n, k, alpha, A, ld, B, ld, beta, C, CRef, CImpl, ld, 1);
                printf("Result is %12s with trans (%s, %s)\n", (r == 0) ? "CORRECT" : "INCORRECT", TRANS_STR[t1], TRANS_STR[t2]);
            }
            xkblas_memory_invalidate_caches();
        }
    }

    # undef A
    # undef B
    # undef C
    # undef CRef
    # undef CImpl

    return 0;
}

TYPED
static int
main_syrk(char ** args)
{
    /* parse arguments */
    CBLAS_UPLO      uplo  = CblasUpper;
    CBLAS_TRANSPOSE trans = CblasNoTrans;
    int n = atoi(args[0]);
    int k = atoi(args[1]);
    int ts = atoi(args[2]);
    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;

    /* currently only support this */
    int ld = MAX(n, k);
    printf("Set (n, k) = (%d, %d)\n", n, k);

    /* allocate matrices */
    uintptr_t matrices[4];
    # define A     ((TYPE *)matrices[0])
    # define C     ((TYPE *)matrices[1])
    # define CRef  ((TYPE *)matrices[2])
    # define CImpl ((TYPE *)matrices[3])
    prepare_n_matrices<P>(matrices, 4, ld, ld);
    FILL(&alpha, 1);
    FILL(&beta, 1);

    /* run on impl */
    int t1 = 0;
    for (int t1 = 0 ; t1 < N_CBLAS_TRANSPOSE ; ++t1)
    {
        if ((TRANS[t1] != CblasNoTrans) && (TRANS[t1] != CblasTrans))
            continue ;

        memcpy(CRef,  C, sizeof(TYPE) * (ld * ld));
        memcpy(CImpl, C, sizeof(TYPE) * (ld * ld));

        printf("Running implementation with (%s)\n", TRANS_STR[t1]);
        uint64_t t0 = get_nanotime();
        set_tile_size(ts);
        xkblas->syrk_async<P>(uplo, TRANS[t1], n, k, &alpha, A, ld, &beta, CImpl, ld);
        xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, CImpl, ld, n, n, sizeof(TYPE));
        uint64_t tt = get_nanotime();
        xkblas_sync();
        uint64_t tf = get_nanotime();
        printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);

        if (!SKIP_CHECK)
        {
            /* check correctness */
            int r = syrk_cmp<P>(uplo, trans, n, k, alpha, A, ld, beta, C, CRef, CImpl, ld, 1);
            printf("Result is %s\n", (r == 0) ? "CORRECT" : "INCORRECT");
        }
    }

    # undef A
    # undef C
    # undef CRef
    # undef CImpl

    return 0;
}

TYPED
static int
main_trsm(char ** args)
{
    /* parse arguments */
    int m = atoi(args[0]);
    int n = atoi(args[1]);
    int ts = atoi(args[2]);
    set_tile_size(ts);
    int m_threshold = atoi(args[3]);
    TYPE alpha = (const TYPE) 1.0;

    /* currently only support this */
    int ld = MAX(m, n);

    printf("Set (m, n) = (%d, %d)\n", m, n);

    /* allocate matrices */
    uintptr_t matrices[4];
    # define A     ((TYPE *)matrices[0])
    # define B     ((TYPE *)matrices[1])
    # define BRef  ((TYPE *)matrices[2])
    # define BImpl ((TYPE *)matrices[3])
    prepare_n_matrices<P>(matrices, 4, ld, ld);
    // FILL(&alpha, 1);

    // diagonal dominante
    TYPE invmax = 1.0 / (TYPE) MAX(m,n);
    for (int i = 0; i < ld*ld ; ++i)
        A[i] *= invmax;
    for(int i = 0 ; i < ld ; ++i)
        A[ld*i+i] = 1.0;

    // we want to optimize this
    int s = 0; // CblasLeft;
    int u = 1; // 0 = CblasUpper; 1 = CblasLower
    int t = 0; // CblasNoTrans;
    int d = 1; // CblasUnit;

    for (int iter = 0 ; iter < 10 ; ++iter)
    {
        //for (int s = 0 ; s < N_CBLAS_SIDE ; ++s)
        {
            //for (int u = 0 ; u < N_CBLAS_UPLO ; ++u)
            {
                //for (int t = 0 ; t < N_CBLAS_TRANSPOSE ; ++t)
                {
                    //for (int d = 0 ; d < N_CBLAS_DIAG ; ++d)
                    {
                        xkblas_memory_invalidate_caches();
                        memcpy(BImpl, B, sizeof(TYPE) * (ld * ld));

                        /* run on impl */
                        if (iter == 0)
                        {
                            printf("Running implementation with (%s, %s, %s, %s)\n",
                                SIDE_STR[s], UPLO_STR[u], TRANS_STR[t], DIAG_STR[d]);
                        }
                        uint64_t t0 = get_nanotime();
                        xkblas->trsm_async<P>(SIDE[s], UPLO[u], TRANS[t], DIAG[d], m, n, &alpha, A, ld, BImpl, ld, m_threshold);
                        xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, BImpl, ld, m, n, sizeof(TYPE));
                        uint64_t t1 = get_nanotime();
                        xkblas_sync();
                        uint64_t tf = get_nanotime();
                        printf("Implementation took %lf s. and creating graph %lf s.\n",
                            (tf - t0) / (double)1e9,
                            (t1 - t0) / (double)1e9
                        );

                        if (iter == 0 && !SKIP_CHECK)
                        {
                            memcpy(BRef,  B, sizeof(TYPE) * (ld * ld));
                            int r = trsm_cmp<P>(SIDE[s], UPLO[u], TRANS[t], DIAG[d], m, n, alpha, A, ld, B, BRef, BImpl, ld);
                            printf("Result is %12s with params (%s, %s, %s, %s)\n", (r == 0) ? "CORRECT" : "INCORRECT", SIDE_STR[s], UPLO_STR[u], TRANS_STR[t], DIAG_STR[d]);
                        }
                    }
                }
            }
        }
    }

    # undef A
    # undef B
    # undef BRef
    # undef BImpl

    return 0;
}

TYPED
static int
main_copyscale(char ** args)
{
    int m  = atoi(args[0]);
    int n  = atoi(args[1]);
    int ts = atoi(args[2]);

    /* currently only support this */
    int ld = MAX(m, n);
    printf("Set (m, n) = (%d, %d)\n", m, n);

    /* allocate matrices */
    uintptr_t matrices[3];
    # define D  ((TYPE *)matrices[0])
    # define L  ((TYPE *)matrices[1])
    # define U  ((TYPE *)matrices[2])
    prepare_n_matrices<P>(matrices, 3, ld, ld);

    /* run on impl */
    printf("Running implementation...\n");
    {
        uint64_t t0 = get_nanotime();
        bool should_copy = true;
        int * IW = NULL;
        set_tile_size(ts);
        xkblas->copyscale_async<P>(m, n, should_copy, IW, D, ld, L, ld, U, ld);
        xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, L, ld, m, n, sizeof(TYPE));
        xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, U, ld, n, m, sizeof(TYPE));
        xkblas_sync();
        uint64_t tf = get_nanotime();
        printf("Implementation took %lf s.\n", (tf - t0) / (double)1e9);
    }

    puts("Correctness check not implemented");

    # undef D
    # undef L
    # undef U

    return 0;
}

TYPED
static int
main_mumps(char ** args)
{

    /**
     *           n         m
     *      .-------.-----------.
     *  n   |   D   |    U      |
     *      |       |           |
     *      .-------.-----------.
     *      |       |           |
     *      |       |           |
     *   m  |  L    |     G     |
     *      |       |           |
     *      |       |           |
     *      .-------.-----------.
     */

    # define USE_WRITE_BACK     1
    # define USE_ARGS_MATRIX    1
    # define USE_TS_TUNER       0
    # define USE_PREALLOCATE    1
    # define NMATRICES          10

    TYPE alpha, beta;
    FILL(&alpha, 1);
    FILL(&beta, 1);

    bool should_copy = true;
    int * IW = NULL;
    uint64_t tt0;

    /* parse arguments */
    int  Ix = atoi(args[0]);

    // MATRIX SIZE TO TEST
    # if USE_ARGS_MATRIX
    int mn[][2] = {
        {atoi(args[1]), atoi(args[2])}
    };
    # else
    int mn[NMATRICES][2];
    for (int k = 0; k < sizeof(mn) / (2 * sizeof(int)); ++k)
    {
        mn[k][0] = 1024 + rand() % (16384 - 1024 + 1);
        mn[k][1] =  512 + rand() % ( 8192 -  512 + 1);
        for (int i = 0 ; i < 2 ; ++i)
            if (mn[k][i] % 2 == 1)
                mn[k][i] += 1;
    }
    # endif /*USE_ARGS_MATRIX */

    constexpr int nmatrices = sizeof(mn) / (2 * sizeof(int));
    TYPE * D_matrices[nmatrices];
    TYPE * L_matrices[nmatrices];
    TYPE * U_matrices[nmatrices];
    TYPE * G_matrices[nmatrices];
    for (int k = 0 ; k < nmatrices ; ++k)
    {
        // do not account for first matrix in measurement
        int  m = mn[k][0];
        int  n = mn[k][1];

        int ld = m+n;

        printf("allocating and filling matrices %d/%d with (m, n) = (%d, %d)\n", k+1, nmatrices, m, n);

        /* allocate matrices */
        # if 0
        uintptr_t matrices[1];
        prepare_n_matrices<P>(matrices, 1, ld);

        TYPE * D = (TYPE *) matrices[0];
        TYPE * L = (TYPE *) (D + n*ld + 0);
        TYPE * U = (TYPE *) (D + 0*ld + n);
        TYPE * G = (TYPE *) (D + n*ld + n);
        # else
        uintptr_t matrices[4];
        prepare_n_matrices<P>(matrices, 4, ld, ld);

        TYPE * D = (TYPE *) matrices[0];
        TYPE * L = (TYPE *) matrices[1];
        TYPE * U = (TYPE *) matrices[2];
        TYPE * G = (TYPE *) matrices[3];
        # endif

        D_matrices[k] = D;
        L_matrices[k] = L;
        U_matrices[k] = U;
        G_matrices[k] = G;

        printf("(init) Dumping some values of G : %lf %lf %lf %lf\n",
                G[0], G[m*m/2], G[m*m*3/4], G[3]);
    }

    // TILE SIZES TO TEST
    # if !USE_TS_TUNER
    int ts1 = atoi(args[3]);
    int ts2 = atoi(args[4]);
    int ts3 = atoi(args[5]);
    int ts[][3] = {
        {ts1, ts2, ts3}
    };
    # else
    int m = mn[0][0];
    int n = mn[0][1];

    int ts[][3] = {
        // {512, 512, 512},
        {1024, 1024, 1024},
        {2048, 2048, 2048},
        {512, 1024, 2048},
        {1024, 2048, 4096},
        {2048, 4096, 8192},
        {m/1, m/2, m/4},
        {n/1, n/2, n/4},
        {m/8, m/16, 2048},
        {1024, m/2, n/4},
        {1024, m/4, 2048},
        {m/1, 1024, 4096},
        {n/2, m/2, 8192},
        {m/8, 512, n/8},
        {m/16, n/16, 1024},
        {4096, 2048, m/2},
        {n/4, m/4, 2048},
        {8192, n/8, m/8},
        {m/2, 512, 1024},
        {n/2, 4096, m/1},
        {m/1, n/2, 512},
        {1024, 1024, 1024},
        {m/2, m/4, m/8},
        {n/4, n/8, n/16},
        {2048, 4096, m/16},
        {8192, n/4, m/4},
        {n/1, 1024, m/2},
        {m/4, 1024, n/4},
        {n/8, 512, m/8},
        {512, 512, 512},
        {2048, 2048, 2048},
        {m/8, m/2, 1024},
        {n/2, n/1, 4096},
        {m/16, m/8, m/4},
        {n/16, n/8, 512},
        {m/2, 2048, 8192},
        {4096, m/1, 2048},
        {n/2, 1024, 1024},
        {n/4, 512, 512},
        {m/1, m/1, m/1},
        {n/1, n/1, n/1},
        {m/2, m/2, m/2},
        {n/2, n/2, n/2},
        {m/4, m/4, m/4},
        {n/4, n/4, n/4},
        {m/8, m/8, m/8},
        {n/8, n/8, n/8},
        {m/16, m/16, m/16},
        {n/16, n/16, n/16},
        {512, n/2, m/8},
        {8192, m/4, n/8},
        {1024, n/4, 2048},
        {4096, n/2, m/2},
        {m/1, 512, n/16},
        {n/1, 8192, m/16},
        {m/2, 2048, 512},
        {m/4, n/2, 1024},
        {512, m/8, 2048},
        {n/2, 4096, 512},
        {m/4, m/8, 4096},
        {8192, n/1, m/2},
        {1024, 4096, n/4},
        {m/8, 2048, n/4},
        {n/1, m/8, 8192},
        {m/16, n/2, 4096},
        {m/1, 8192, 1024},
        {n/4, 2048, m/4},
        {m/2, 1024, 512},
        {m/4, 512, n/2},
        {1024, m/16, n/8},
        {2048, 4096, 512},
        {8192, m/1, n/16},
        {n/2, 1024, 512},
        {m/4, 4096, 512},
        {m/16, 1024, n/4},
        {m/8, 8192, 2048},
        {1024, 512, n/2},
        {m/2, 4096, n/4},
        {n/1, 1024, m/8},
        {2048, n/2, m/16},
        {m/1, n/2, n/4},
        {m/2, 8192, 1024},
        {m/4, n/1, 4096},
        {n/4, 1024, 2048},
        {m/2, 2048, n/1}
    };
    # endif

    // for each matrices
    for (int k = 0 ; k < nmatrices ; ++k)
    {
        TYPE * D = D_matrices[k];
        TYPE * L = L_matrices[k];
        TYPE * U = U_matrices[k];
        TYPE * G = G_matrices[k];

        int  m = mn[k][0];
        int  n = mn[k][1];
        int ld = m+n;
        printf("running matrices with (m, n) = (%d, %d)\n", m, n);

        uint64_t tmin = UINT64_MAX;
        int imin = 0;
        const int ntiles = sizeof(ts) / (3 * sizeof(int));
        for (int i = MUMPS_AUTOTUNE_FROM ; i < ntiles ; ++i)
        {
            for (int j = 0 ; j < Ix ; ++j)
            {
                if (ts[i][0] < 200 || ts[i][1] < 200 || ts[i][2] < 200)
                    continue ;
                if (ts[i][0] > n || ts[i][1] > n || ts[i][2] > n)
                    continue ;

                # if USE_PREALLOCATE
                preallocate<P>(D, n, n, ld);
                preallocate<P>(L, m, n, ld);
                preallocate<P>(U, n, m, ld);
                preallocate<P>(G, m, m, ld);
                # endif /* USE_PREALLOCATE */

                uint64_t t0 = get_nanotime();

                set_tile_size(ts[i][0]);
                xkblas->trsm_async<P>(CblasLeft, CblasUpper, CblasTrans, CblasUnit, n, m, &alpha, D, ld, L, ld);

                set_tile_size(ts[i][1]);
                xkblas->copyscale_async<P>(m, n, should_copy, IW, D, ld, L, ld, U, ld);

                # if USE_WRITE_BACK
                // xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, D, ld, m, m, sizeof(TYPE));
                xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, L, ld, n, m, sizeof(TYPE));
                xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, U, ld, m, n, sizeof(TYPE));
                # endif
                set_tile_size(ts[i][2]);
                xkblas->gemmt_async<P>(CblasUpper, CblasNoTrans, CblasNoTrans, m, n, &alpha, U, ld, L, ld, &beta, G, ld);

                # if USE_WRITE_BACK
                xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, G, ld, m, m, sizeof(TYPE));
                # endif

                uint64_t tt = get_nanotime();
                xkblas_sync();
                uint64_t tf = get_nanotime();
                printf("(tiling %d/%u) (iter %d/%d) Compute took %lf s. (graph construction took %lf s.) - with ts = {%d, %d, %d}\n", i, ntiles, j, Ix, (tf-t0)/1e9, (tt-t0)/1e9, ts[i][0], ts[i][1], ts[i][2]);

                xkblas_memory_invalidate_caches();

                if (tf - t0 < tmin)
                {
                    imin = i;
                    tmin = tf - t0;
                }

                // start accounting for time after 1st iteration
                if (k == 0 && i == 0 && j == 0)
                    tt0 = get_nanotime();
            }
        }

        printf("Best perf obtained with ts = {%d, %d, %d} for %lf s\n",
                ts[imin][0], ts[imin][1], ts[imin][2], (double)tmin/(double)1e9);
        printf("(done) Dumping some values of G : %lf %lf %lf %lf\n", G[0], G[m*m/2], G[m*m*3/4], G[3]);

    }
    uint64_t ttf = get_nanotime();
    printf("Total transfer+compute took %lf s. (excluding first matrix)\n", (ttf-tt0)/(double)1e9);

    return 0;
}

TYPED
static int
main_trsm_copyscale_gemm(char ** args)
{
    /* parse arguments */
    int m = atoi(args[0]);
    int n = atoi(args[1]);
    int k = atoi(args[2]);
    int ts = atoi(args[3]);
    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;
    int ld = MAX(MAX(m, n), k);

    /* allocate matrices */
    uintptr_t matrices[4];
    # define D  ((TYPE *)matrices[0])
    # define L  ((TYPE *)matrices[1])
    # define U  ((TYPE *)matrices[2])
    # define G  ((TYPE *)matrices[3])
    prepare_n_matrices<P>(matrices, 4, ld, ld);
    FILL(&alpha, 1);
    FILL(&beta, 1);

    int s = 0;
    int u = 0;
    int d = 0;
    int t = 0;
    bool should_copy = true;
    int * IW = NULL;

    // trsm en CblasLeft, CblasUpper, CBlasTrans, CBlasUnit et le gemm CBlasNoTrans, CBlasNoTrans
    int t1 = 0;
    int t2 = 0;

//    for (int t1 = 0 ; t1 < N_CBLAS_TRANSPOSE ; ++t1)
    {
        // for (int t2 = 0 ; t2 < N_CBLAS_TRANSPOSE ; ++t2)
        {
            xkblas_memory_invalidate_caches();
            set_tile_size(ts);

            printf("Running implementation with (%s, %s)\n", TRANS_STR[t1], TRANS_STR[t2]);
            uint64_t t0 = get_nanotime();
            assert(m == n);
            xkblas->trsm_async<P>(SIDE[s], UPLO[u], TRANS[t], DIAG[d], m, k, &alpha, D, ld, L, ld);
            xkblas->copyscale_async<P>(m, k, should_copy, IW, D, ld, L, ld, U, ld);
            xkblas->gemm_async<P>(TRANS[t1], TRANS[t2], m, n, k, &alpha, L, ld, U, ld, &beta, G, ld);

            xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, D, ld, k, k, sizeof(TYPE));
            xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, L, ld, m, k, sizeof(TYPE));
            xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, U, ld, k, n, sizeof(TYPE));
            xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, G, ld, m, n, sizeof(TYPE));

            uint64_t tt = get_nanotime();
            xkblas_sync();
            uint64_t tf = get_nanotime();
            printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);
        }
    }

    # undef D
    # undef L
    # undef U
    # undef G

    return 0;
}

TYPED
static int
main_spmv(char ** args)
{

    /**
     *         n
     *      .-------.  .-.                 m
     *      |       |  | |           _____________
     *      |       |  |X| n    =   [______Y______]
     *   m  |  A    |  | |
     *      |       |  .-.
     *      |       |
     *      .-------.
     */

    TYPE alpha, beta;
    alpha = (TYPE) atof(args[3]);
    beta  = (TYPE) atof(args[4]);
    // FILL(&alpha, 1);
    // FILL(&beta, 1);
    printf("alpha=%.4lf, beta=%.4lf\n", alpha, beta);

    /* parse arguments */
    int m  = atoi(args[0]);
    int n  = atoi(args[1]);
    int ts = atoi(args[2]);

    /* generate matrix A */
    int nnz;
    int * csr_row_offsets, * csr_col_indices;
    TYPE * csr_values;
    const double density = (double) atof(args[5]);
    prepare_csr_matrix<P>(density, m, n, &nnz, &csr_row_offsets, &csr_col_indices, &csr_values);
    dump_csr_matrix<P>("A", m, n, csr_row_offsets, csr_col_indices, csr_values);

    /* generate vectors X and Y */
    /* allocate vectors */
    uintptr_t ptr[3];
    prepare_n_vectors<P>(ptr, 3, MAX(m, n));
    # define X ((TYPE *)ptr[0])
    # define Y ((TYPE *)ptr[1])
    # define Y_check ((TYPE *)ptr[2])
    dump_vector<P>("X", X, n);
    dump_vector<P>("Y", Y, m);
    memcpy(Y_check, Y, m * sizeof(TYPE));

    uint64_t t0 = get_nanotime();

    /* run spmv */
    set_tile_size(ts);
    const int index_base = 0;
    xkblas->spmv_async<P, I32>(&alpha, CblasNoTrans, index_base, m, n, nnz, CblasSparseCSR, csr_row_offsets, csr_col_indices, csr_values, X, &beta, Y);
    xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, Y, m * sizeof(TYPE));
    xkblas->sync();
    uint64_t tf = get_nanotime();
    dump_vector<P>("Y := alpha.A.X + beta.Y", Y, m);


    if (!SKIP_CHECK)
    {
        /* run check */
        int r = spmv_cpu<P>(&alpha, CblasNoTrans, m, n, nnz, csr_row_offsets, csr_col_indices, csr_values, X, &beta, Y_check, Y);
        dump_vector<P>("CPU value of Y", Y_check, m);
        printf("Result is %12s\n", (r == 0) ? "CORRECT" : "INCORRECT");
    }


    printf("Took %lf s\n", (tf - t0) / 1.0e9);

    # undef X
    # undef Y

    return 0;
}

TYPED
static int
main_gemv(char ** args)
{
    TYPE alpha, beta;
    alpha = (TYPE) atof(args[3]);
    beta  = (TYPE) atof(args[4]);
    // FILL(&alpha, 1);
    // FILL(&beta, 1);
    printf("alpha=%.4lf, beta=%.4lf\n", alpha, beta);

    /* parse arguments */
    int m  = atoi(args[0]);
    int n  = atoi(args[1]);
    int ts = atoi(args[2]);

    /* generate matrix A */
    /* allocate matrices */
    uintptr_t matrices[1];
    int ld = MAX(m, n);
    # define A ((TYPE *)matrices[0])
    prepare_n_matrices<P>(matrices, 1, ld, ld);
    dump_matrix<P>("A", A, m, n, ld);

    /* allocate vectors */
    uintptr_t ptr[2];
    prepare_n_vectors<P>(ptr, 2, MAX(m, n));
    # define X ((TYPE *)ptr[0])
    # define Y ((TYPE *)ptr[1])
    dump_vector<P>("X", X, n);
    dump_vector<P>("Y", Y, m);

    uint64_t t0 = get_nanotime();

    /* run spmv */
    set_tile_size(ts);
    xkblas->gemv_async<P>(CblasNoTrans, m, n, &alpha, A, ld, X, 1, &beta, Y, 1);
    xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, Y, m * sizeof(TYPE));
    xkblas->sync();
    uint64_t tf = get_nanotime();
    dump_vector<P>("y := alpha.A.X + beta.y", Y, m);

    printf("Took %lf s\n", (tf - t0) / 1.0e9);

    # undef X
    # undef Y
    # undef A

    return 0;
}

TYPED
static int
main_dot(char ** args)
{
    int n  = atoi(args[0]);
    int ts = atoi(args[1]);
    int incx = 1;
    int incy = 1;

    /* allocate vectors */
    uintptr_t ptr[2];
    prepare_n_vectors<P>(ptr, 2, n);
    # define X ((TYPE *)ptr[0])
    # define Y ((TYPE *)ptr[1])
    dump_vector<P>("X", X, n);
    dump_vector<P>("Y", Y, n);

    TYPE r = 0.0;

    set_tile_size(ts);

    uint64_t t0 = get_nanotime();
    xkblas->dot_async<P>(n, X, incx, Y, incy, &r);
    xkblas->sync();
    uint64_t tf = get_nanotime();

    printf("r_gpu = %lf\n", r);
    printf("GPU Took %lf s\n", (tf - t0) / 1.0e9);

    if (!SKIP_CHECK)
    {
        TYPE r_cpu = (TYPE) 0.0;
        uint64_t t0 = get_nanotime();
        dot_cpu<P>(n, X, incx, Y, incy, &r_cpu);
        uint64_t tf = get_nanotime();
        printf("r_cpu = %lf\n", r_cpu);
        printf("CPU Took %lf s\n", (tf - t0) / 1.0e9);
    }

    # undef X
    # undef Y

    return 0;
}

TYPED
static int
main_scal(char ** args)
{
    int n  = atoi(args[0]);
    int ts = atoi(args[1]);
    TYPE alpha = (TYPE) atof(args[2]);
    printf("alpha=%lf\n", alpha);

    /* allocate vectors */
    uintptr_t ptr[1];
    prepare_n_vectors<P>(ptr, 1, n);
    # define X ((TYPE *)ptr[0])
    dump_vector<P>("X", X, n);

    int incx = 1;
    set_tile_size(ts);

    uint64_t t0 = get_nanotime();
    xkblas->scal_async<P>(n, &alpha, X, incx);
    xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, X, sizeof(TYPE) * n);
    xkblas->sync();
    uint64_t tf = get_nanotime();

    dump_vector<P>("alpha.X", X, n);
    printf("GPU Took %lf s\n", (tf - t0) / 1.0e9);

    # undef X

    return 0;
}


// PIN-GEMM-UNPIN //
TYPED
static int
main_pin_gemm_unpin(char ** args)
{
    /* parse arguments */
    int n = atoi(args[0]);
    int ts = atoi(args[1]);
    int npin = atoi(args[2]);
    int repeat = atoi(args[3]);
    set_tile_size(ts);

    TYPE alpha = (const TYPE) 1.0;
    TYPE beta  = (const TYPE) 1.0;

    /* currently only support this */
    int ld = n;

    /* allocate matrices */
    uintptr_t matrices[3];
    # define A ((TYPE *)matrices[0])
    # define B ((TYPE *)matrices[1])
    # define C ((TYPE *)matrices[2])
    prepare_n_matrices<P>(matrices, 3, ld, ld);

    # define SYNC 0

    for (int i = 0 ; i < repeat ; ++i)
    {
        // assume A and B already on device 1
        premove<P>(A, n, n, ld);
        premove<P>(B, n, n, ld);

        // pin C, run gemm, write back, unpin C
        uint64_t t0 = get_nanotime();
        xkblas->memory_register_async(C, n*n*sizeof(TYPE), npin);

        # if SYNC
        xkblas->sync();
        uint64_t t1 = get_nanotime();
        printf("Register took %lf s\n", (t1-t0)/1e9);
        # endif

        xkblas->gemm_async<P>(CblasNoTrans, CblasNoTrans, n, n, n, &alpha, A, ld, B, ld, &beta, C, ld);

        # if SYNC
        xkblas->sync();
        uint64_t t2 = get_nanotime();
        printf("Gemm took %lf s\n", (t2-t1)/1e9);
        # endif

        xkblas->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, C, ld, n, n, sizeof(TYPE));

        # if SYNC
        xkblas->sync();
        uint64_t t3 = get_nanotime();
        printf("Coherent took %lf s\n", (t3-t2)/1e9);
        # endif

        xkblas->memory_unregister_async(C, n*n*sizeof(TYPE), npin);

        # if SYNC
        xkblas->sync();
        uint64_t t4 = get_nanotime();
        printf("Unregister took %lf s\n", (t4-t3)/1e9);
        # endif

        uint64_t tt = get_nanotime();
        xkblas_sync();
        uint64_t tf = get_nanotime();

        printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);

        xkblas_memory_invalidate_caches();
    }

    # undef C

    return 0;
}

typedef struct  func_t
{
    const char * name;
    int (*f)(char **);
    int nargs;
    const char * descr;
    const char * usage;
}               func_t;

static func_t funcs[] = {
# if 0
    {
        .name = "TRSM-COPYSCALE-GEMM",
        .f = main_trsm_copyscale_gemm,
        .nargs = 5,
        .usage =    "ITER MODE M N S\n"
                    "  - ITER   : number of iterations\n"
                    "  - MODE   : '0' to re-use the same matrices, '1' to use new matrices on each iteration\n"
                    "  - M      : number of rows of matrices L and G, and number of cols of U and G\n"
                    "  - N      : number of rows of matrices D and U, cols of matrices D and L\n"
                    "  - S      : number of rows and cols per tile\n"
    },
# endif
    {
        .name = "AXPY",
        .f = main_axpy<PRECISION>,
        .nargs = 2,
        .descr = "axpy - Y := a.X + Y",
        .usage =    "n TS\n"
                    "  - n  : number of rows of X and Y\n"
                    "  - TS : tile size\n"
    },
    {
        .name = "COPYSCALE",
        .f = main_copyscale<PRECISION>,
        .nargs = 3,
        .descr = "???",
        .usage =    "M N BS_M BS_N\n"
                    "  - M  : number of rows of matrices L, and number of cols of U\n"
                    "  - N  : number of rows of matrices D and U, cols of matrices D and L\n"
                    "  - TS : tile size\n"
    },
    {
        .name = "GEMM",
        .f = main_gemm<PRECISION>,
        .nargs = 4,
        .descr = "C := A.B + C",
        .usage =    "M N K TS\n"
                    "  - M    : n° of rows of A and C\n"
                    "  - N    : n° of cols of B and C\n"
                    "  - K    : n° of cols of A, rows of B\n"
                    "  - BS_N : tile size\n"
    },

    {
        .name = "SYRK",
        .f = main_syrk<PRECISION>,
        .nargs = 3,
        .descr = "C := A.A^T + C",
        .usage =    "N K TS\n"
                    "  - N  : n° of rows of A and C\n"
                    "  - K  : n° of cols of A\n"
                    "  - TS : tile size\n"
    },

    {
        .name = "TRSM",
        .f = main_trsm<PRECISION>,
        .nargs = 4,
        .descr = "A.X = B",
        .usage =    "M N TS THRESHOLD\n"
                    "  -         M : number of rows of matrices L\n"
                    "  -         N : number of rows of matrices D, cols of matrices D and L\n"
                    "  -        TS : tile size\n"
                    "  - THRESHOLD : minimum value of 'm' to recurse before running a tiled-TRSM\n"
    },

    {
        .name = "GEMM-GEMM",
        .f = main_gemm_gemm<PRECISION>,
        .nargs = 5,
        .descr = "C := a.A.B + b.C - repeat twice with two block sizes",
        .usage =    "M N K TS1 TS2\n"
                    "  - M    : n° of rows of A and C\n"
                    "  - N    : n° of cols of B and C\n"
                    "  - K    : n° of cols of A, rows of B\n"
                    "  - TS1  : 1st gemm block size\n"
                    "  - TS1  : 2nd gemm block size\n"
    },


    {
        .name = "TRSM-COPYSCALE-GEMM",
        .f = main_trsm_copyscale_gemm<PRECISION>,
        .nargs = 4,
        .descr = "???",
        .usage =    "M N K TS\n"
                    "  - M  : n° of rows of A and C\n"
                    "  - N  : n° of cols of B and C\n"
                    "  - K  : n° of cols of A, rows of B\n"
                    "  - TS : tile size\n"
    },

    {
        .name = "MUMPS",
        .f = main_mumps<PRECISION>,
        .nargs = 6,
        .descr = "???",
        .usage =    "I M N K TS\n"
                    "  -   I : number of iterations\n"
                    "  -   M : ?\n"
                    "  -   N : ?\n"
                    "  - TS1 : tile size for trsm\n"
                    "  - TS2 : tile size for copyscale\n"
                    "  - TS3 : tile size for gemm\n",
    },

    {
        .name = "SPMV",
        .f = main_spmv<PRECISION>,
        .nargs = 6,
        .descr = "SPMV Y := alpha.A.X + beta.X",
        .usage =    "M N TS alpha beta density\n"
                    "  -       M : number of rows\n"
                    "  -       N : number of columns\n"
                    "  -      TS : tile size\n"
                    "  -   alpha : alpha\n"
                    "  -    beta : beta\n"
                    "  - density : beta\n"
    },

    {
        .name = "GEMV",
        .f = main_gemv<PRECISION>,
        .nargs = 5,
        .descr = "y := alpha.A.x + beta.y",
        .usage =    "m n ts alpha beta\n"
                    "  -       m : number of rows\n"
                    "  -       n : number of columns\n"
                    "  -      ts : tile size\n"
                    "  -   alpha : alpha\n"
                    "  -    beta : beta\n"
    },

    {
        .name = "DOT",
        .f = main_dot<PRECISION>,
        .nargs = 2,
        .descr = "r := x.y",
        .usage =    "n\n"
                    "  -  n : vector sizes\n"
                    "  - ts : tile size\n"
    },

    {
        .name = "SCAL",
        .f = main_scal<PRECISION>,
        .nargs = 3,
        .descr = "x := alpha.x",
        .usage =    "n\n"
                    "  -     n : vector size\n"
                    "  -    ts : tile size\n"
                    "  - alpha : alpha\n"
    },

    {
        .name = "PIN-GEMM-UNPIN",
        .f = main_pin_gemm_unpin<PRECISION>,
        .nargs = 4,
        .descr = "Pre-move A and B to the GPU, and do a gemm(A, B, C) with overlap between pining and operations on C",
        .usage = "N TS NPIN REPEAT\n"
                 "  -       N : number of rows and columns\n"
                 "  -      TS : tile size\n"
                 "  -    NPIN : number of segments to pin the matrix C\n"
                 "  -  REPEAT : number of time to repeat the sequence\n"
    }

};
# define N_FUNCS (sizeof(funcs) / sizeof(func_t))

static int
error_usage(const char * label, func_t * func)
{
    if (func == NULL)
    {
        fprintf(stderr, "usage : %s [PRECISION] [FUNC] [...]\n", label);
        fprintf(stderr, "  - [FUNC] is one of");
        for (unsigned int i = 0 ; i < N_FUNCS ; ++i)
            fprintf(stderr, " %s", funcs[i].name);
        fprintf(stderr, "\n");
        fprintf(stderr, "  - [...] are [FUNC] parameters\n");
    }
    else
    {
        fprintf(stderr, "usage : %s %s ", label, func->name);
        fprintf(stderr, "%s", func->usage);
        fprintf(stderr, "to compute %s\n", func->descr);
    }
    return 1;
}

int
main(int argc, char ** argv)
{
    SKIP_CHECK = getenv("SKIP_CHECK") ? atoi(getenv("SKIP_CHECK")) : SKIP_CHECK;
    ALIGN_MATRICES = getenv("ALIGN_MATRICES") ? atoi(getenv("ALIGN_MATRICES")) : ALIGN_MATRICES;
    REGISTER_MEMORY = getenv("REGISTER_MEMORY") ? atoi(getenv("REGISTER_MEMORY")) : REGISTER_MEMORY;
    DUMP_GRAPH = getenv("DUMP_GRAPH") ? atoi(getenv("DUMP_GRAPH")) : DUMP_GRAPH;
    MUMPS_AUTOTUNE_FROM = getenv("MUMPS_AUTOTUNE_FROM") ? atoi(getenv("MUMPS_AUTOTUNE_FROM")) : MUMPS_AUTOTUNE_FROM;
    printf("Skipping checks (SKIP_CHECK) %s\n", SKIP_CHECK ? "enabled" : "disabled");
    printf("Align matrices (ALIGN_MATRICES) %s\n", ALIGN_MATRICES ? "enabled" : "disabled");
    printf("Register memory (REGISTER_MEMORY) %s\n", REGISTER_MEMORY ? "enabled" : "disabled");
    printf("Dumping graph (DUMP_GRAPH) %s\n", DUMP_GRAPH ? "enabled" : "disabled");

    srand(0x16112003);

    if (argc >= 2)
    {
        int    nargs = argc - 1;
        char ** args = argv + 1;

        for (unsigned int i = 0 ; i < N_FUNCS ; ++i)
        {
            func_t * func = funcs + i;
            if (strcmp(func->name, args[0]) == 0)
            {
                --nargs;
                ++args;
                if (nargs != func->nargs)
                    return error_usage(argv[0], func);

                xkblas_init();
                xkblas = xkblas_get();
                int r = func->f(args);

                # if XKRT_SUPPORT_DEBUG
                if (DUMP_GRAPH)
                {
                    xkrt::thread_t * thread = xkrt::thread_t::get_tls();
                    FILE * f = fopen("graph.dot", "w");
                    thread->dump_tasks(f);
                    fclose(f);
                    printf("Graph dumped\n");
                }
                # endif

                xkblas_deinit();
                return r;
            }
        }
    }

    error_usage(argv[0], NULL);
    return 1;
}
