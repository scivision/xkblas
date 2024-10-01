

// TODO : Add tests for each parameter combination possible (e.g.
// CblasTrans/CblansNoTrans, CblasLeft/CblasRight, etc...

extern "C" {

    # include <stdio.h>
    # include <stdlib.h>
    # include <stdint.h>
    # include <string.h>
    # include <time.h>
};

# include "common/blas.h"
# include "common/blas-version.h"
# include "common/impl.hpp"
# include "common/time.cc"
# include "common/utils.cc"

//////////////////////////////
//  TARGETED IMPLEMENTATION //
//////////////////////////////

# include "xkblas/impl.cc"
# include "common/check.cc"
static impl_t impl;

//////////////////////
//  CLI TO RUN-TIME //
//////////////////////

// GEMM - GEMM //
static int
main_gemm_gemm(char ** args)
{
    /* parse arguments */
    CBLAS_TRANSPOSE transA = CblasNoTrans;
    CBLAS_TRANSPOSE transB = CblasNoTrans;
    int m     = atoi(args[0]);
    int n     = atoi(args[1]);
    int k     = atoi(args[2]);
    int bs1_m = atoi(args[3]);
    int bs1_n = atoi(args[4]);
    int bs2_m = atoi(args[5]);
    int bs2_n = atoi(args[6]);

    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;

    /* currently only support this */
    printf("Set (m, n, k) = (%d, %d, %d)\n", m, n, k);
    int ld = MAX(MAX(m, n), k);
    uintptr_t alignon = sizeof(TYPE) * ld;

    /* allocate matrices */
    uintptr_t mem = (uintptr_t) malloc(alignon + 5 * sizeof(TYPE) * (ld * ld));
    uintptr_t Ap     = mem + (alignon - (mem % alignon)) + 0 * sizeof(TYPE) * (ld * ld);
    uintptr_t Bp     = mem + (alignon - (mem % alignon)) + 1 * sizeof(TYPE) * (ld * ld);
    uintptr_t Cp     = mem + (alignon - (mem % alignon)) + 2 * sizeof(TYPE) * (ld * ld);
    uintptr_t CpRef  = mem + (alignon - (mem % alignon)) + 3 * sizeof(TYPE) * (ld * ld);
    uintptr_t CpImpl = mem + (alignon - (mem % alignon)) + 4 * sizeof(TYPE) * (ld * ld);

    assert(Ap     % alignon == 0);
    assert(Bp     % alignon == 0);
    assert(Cp     % alignon == 0);
    assert(CpRef  % alignon == 0);
    assert(CpImpl % alignon == 0);

    TYPE * A     = (TYPE *) Ap;
    TYPE * B     = (TYPE *) Bp;
    TYPE * C     = (TYPE *) Cp;
    TYPE * CRef  = (TYPE *) CpRef;
    TYPE * CImpl = (TYPE *) CpImpl;

    /* initialize matrices */
    FILL(A, 3*ld*ld); // fill A, B and C
    FILL(&alpha, 1);
    FILL(&beta, 1);

    memcpy(CRef,  C, sizeof(TYPE) * (ld * ld));
    memcpy(CImpl, C, sizeof(TYPE) * (ld * ld));

    /* run on impl */
    printf("Running implementation...\n");
    {
        uint64_t t0 = get_nanotime();

        impl.set_tile(bs1_m, bs1_n);
        impl.gemm(transA, transB, m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);

        impl.set_tile(bs2_m, bs2_n);
        impl.gemm(transA, transB, m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);

        impl.coherent(CImpl, m, n, ld);

        uint64_t tt = get_nanotime();
        impl.wait();
        uint64_t tf = get_nanotime();
        printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);
    }

    /* check correctness */
    int r = gemm_cmp(transA, transB, m, n, k, alpha, A, ld, B, ld, beta, C, CRef, CImpl, ld, 2);
    if (r == 0)
        puts("Result is CORRECT");
    else
        puts("Result is INCORRECT !!");

    return 0;
}


// GEMM //
static int
main_gemm(char ** args)
{
    /* parse arguments */
    CBLAS_TRANSPOSE transA = CblasNoTrans;
    CBLAS_TRANSPOSE transB = CblasNoTrans;
    int m = atoi(args[0]);
    int n = atoi(args[1]);
    int k = atoi(args[2]);
    int bs_m = atoi(args[3]);
    int bs_n = atoi(args[4]);
    TYPE alpha = (const TYPE) 0.0;
    TYPE beta  = (const TYPE) 0.0;

    /* currently only support this */
    int ld = MAX(MAX(m, n), k);

    printf("Set (m, n, k) = (%d, %d, %d)\n", m, n, k);

    uintptr_t alignon = sizeof(TYPE) * ld;

    /* allocate matrices */
    uintptr_t mem = (uintptr_t) malloc(alignon + 5 * sizeof(TYPE) * (ld * ld));
    uintptr_t Ap     = mem + (alignon - (mem % alignon)) + 0 * sizeof(TYPE) * (ld * ld);
    uintptr_t Bp     = mem + (alignon - (mem % alignon)) + 1 * sizeof(TYPE) * (ld * ld);
    uintptr_t Cp     = mem + (alignon - (mem % alignon)) + 2 * sizeof(TYPE) * (ld * ld);
    uintptr_t CpRef  = mem + (alignon - (mem % alignon)) + 3 * sizeof(TYPE) * (ld * ld);
    uintptr_t CpImpl = mem + (alignon - (mem % alignon)) + 4 * sizeof(TYPE) * (ld * ld);

    assert(Ap     % alignon == 0);
    assert(Bp     % alignon == 0);
    assert(Cp     % alignon == 0);
    assert(CpRef  % alignon == 0);
    assert(CpImpl % alignon == 0);

    TYPE * A     = (TYPE *) Ap;
    TYPE * B     = (TYPE *) Bp;
    TYPE * C     = (TYPE *) Cp;
    TYPE * CRef  = (TYPE *) CpRef;
    TYPE * CImpl = (TYPE *) CpImpl;

    /* initialize matrices */
    FILL(A, 3*ld*ld); // fill A, B and C
    FILL(&alpha, 1);
    FILL(&beta, 1);

    memcpy(CRef,  C, sizeof(TYPE) * (ld * ld));
    memcpy(CImpl, C, sizeof(TYPE) * (ld * ld));

    /* run on impl */
    printf("Running implementation...\n");
    {
        uint64_t t0 = get_nanotime();
        impl.set_tile(bs_m, bs_n);
        impl.gemm(transA, transB, m, n, k, &alpha, A, ld, B, ld, &beta, CImpl, ld);
        impl.coherent(CImpl, m, n, ld);
        uint64_t tt = get_nanotime();
        impl.wait();
        uint64_t tf = get_nanotime();
        printf("Implementation took %lf s. (graph construction took %lf s.)\n", (tf-t0)/1e9, (tt-t0)/1e9);
    }

    /* check correctness */
    int r = gemm_cmp(transA, transB, m, n, k, alpha, A, ld, B, ld, beta, C, CRef, CImpl, ld, 1);
    if (r == 0)
        puts("Result is CORRECT");
    else
        puts("Result is INCORRECT !!");

    return 0;
}

static int
main_trsm(char ** args)
{
    /* parse arguments */
    CBLAS_SIDE side = CblasLeft;
    CBLAS_UPLO uplo = CblasUpper;
    CBLAS_DIAG diag = CblasNonUnit;
    CBLAS_TRANSPOSE transA = CblasNoTrans;

    int m = atoi(args[0]);
    int n = atoi(args[1]);
    int bs_m = atoi(args[2]);
    int bs_n = atoi(args[3]);
    TYPE alpha = (const TYPE) 0.0;

    /* currently only support this */
    int ld = MAX(m, n);

    printf("Set (m, n) = (%d, %d)\n", m, n);

    uintptr_t alignon = sizeof(TYPE) * ld;

    /* allocate matrices */
    uintptr_t mem = (uintptr_t) malloc(alignon + 4 * sizeof(TYPE) * (ld * ld));
    uintptr_t Ap     = mem + (alignon - (mem % alignon)) + 0 * sizeof(TYPE) * (ld * ld);
    uintptr_t Bp     = mem + (alignon - (mem % alignon)) + 1 * sizeof(TYPE) * (ld * ld);
    uintptr_t BpRef  = mem + (alignon - (mem % alignon)) + 2 * sizeof(TYPE) * (ld * ld);
    uintptr_t BpImpl = mem + (alignon - (mem % alignon)) + 3 * sizeof(TYPE) * (ld * ld);

    assert(Ap     % alignon == 0);
    assert(Bp     % alignon == 0);
    assert(BpRef  % alignon == 0);
    assert(BpImpl % alignon == 0);

    TYPE * A     = (TYPE *) Ap;
    TYPE * B     = (TYPE *) Bp;
    TYPE * BRef  = (TYPE *) BpRef;
    TYPE * BImpl = (TYPE *) BpImpl;

    /* initialize matrices */
    FILL(A, ld*ld);
    FILL(B, ld*ld);
    FILL(&alpha, 1);

    // diagonal dominante
    TYPE invmax = 1.0 / (TYPE) MAX(m,n);
    for (int i = 0; i < ld*ld ; ++i)
        A[i] *= invmax;
    for(int i = 0 ; i < ld ; ++i)
        A[ld*i+i] = 1.0;

    memcpy(BRef,  B, sizeof(TYPE) * (ld * ld));
    memcpy(BImpl, B, sizeof(TYPE) * (ld * ld));

    /* run on impl */
    printf("Running implementation...\n");
    {
        uint64_t t0 = get_nanotime();
        impl.set_tile(bs_m, bs_n);
        impl.trsm(side, uplo, transA, diag, m, n, &alpha, A, ld, BImpl, ld);
        impl.coherent(BImpl, m, n, ld);
        impl.wait();
        uint64_t tf = get_nanotime();
        printf("Implementation took %lf s.\n", (tf - t0) / (double)1e9);
    }

    /* check correctness */
    int r = trsm_cmp(side, uplo, transA, diag, m, n, alpha, A, ld, B, BRef, BImpl, ld);
    if (r == 0)
        puts("Result is CORRECT");
    else
        puts("Result is INCORRECT !!");

    return 0;
}

static int
main_copyscale(char ** args)
{
    int m    = atoi(args[0]);
    int n    = atoi(args[1]);
    int bs_m = atoi(args[2]);
    int bs_n = atoi(args[3]);

    /* currently only support this */
    int ld = MAX(m, n);
    printf("Set (m, n) = (%d, %d)\n", m, n);
    uintptr_t alignon = sizeof(TYPE) * ld;

    /* allocate matrices */
    uintptr_t mem = (uintptr_t) malloc(alignon + 3 * sizeof(TYPE) * (ld * ld));
    uintptr_t Dp  = mem + (alignon - (mem % alignon)) + 0 * sizeof(TYPE) * (ld * ld);
    uintptr_t Lp  = mem + (alignon - (mem % alignon)) + 1 * sizeof(TYPE) * (ld * ld);
    uintptr_t Up  = mem + (alignon - (mem % alignon)) + 2 * sizeof(TYPE) * (ld * ld);

    assert(Dp % alignon == 0);
    assert(Lp % alignon == 0);
    assert(Up % alignon == 0);

    TYPE * D = (TYPE *) Dp;
    TYPE * L = (TYPE *) Lp;
    TYPE * U = (TYPE *) Up;

    /* initialize matrices */
    FILL(D, 3*ld*ld);

    /* run on impl */
    printf("Running implementation...\n");
    {
        uint64_t t0 = get_nanotime();
        bool should_copy = true;
        int * IW = NULL;
        impl.set_tile(bs_m, bs_n);
        impl.copyscale(m, n, should_copy, IW, D, ld, L, ld, U, ld);
        impl.coherent(L, m, n, ld);
        impl.coherent(U, n, m, ld);
        impl.wait();
        uint64_t tf = get_nanotime();
        printf("Implementation took %lf s.\n", (tf - t0) / (double)1e9);
    }

    puts("Correctness check not implemented");

    return 0;
}

# if 0
static int
main_copyscale(char ** args)
{
    assert(0);

    int M  = atoi(args[0]);
    int N  = atoi(args[1]);
    int ld = N + M;
    int S = atoi(args[2]);

    if (ld < M || ld < N)
    {
        fprintf(stderr, "ld must be greated than M and N\n");
        return 1;
    }
    if (M % S || N % S)
    {
        fprintf(stderr, "Tile sizes must divide matrix sizes");
        return 1;
    }

    int D = 0;
    int L = ld * N;
    int U = N;

    printf("Set (M, N, ld) = (%d, %d, %d) with tile (%d, %d)\n", M, N, ld, S, S);

    uint64_t t0 = get_nanotime();
    // TODO
    uint64_t tf = get_nanotime();

    printf("Took %lf s.\n", (tf - t0) / (double)1e9);

    return 0;
}

static int
main_trsm_copyscale_gemm(char ** args)
{
    assert(0);

    int N_ITER  = atoi(args[0]);
    int MODE    = atoi(args[1]);
    int M       = atoi(args[2]);
    int N       = atoi(args[3]);
    int S       = atoi(args[4]);

    # if 0
    int S_trsm      = (int)(S*1.25);
    int S_copyscale = (int)(S*2.00);
    int S_gemm      = (int)(S*0.75);
    # else
    int S_trsm      = S;
    int S_copyscale = S;
    int S_gemm      = S;
    # endif

    if (M % S || N % S)
    {
        fprintf(stderr, "Tile sizes must divide matrix sizes");
        return 1;
    }

    int ld = N + M;
    printf("Set (M, N, ld) = (%d, %d, %d) with tiles (S, trsm, copyscale, gemm)=(%d, %d, %d, %d)\n", M, N, ld, S, S_trsm, S_copyscale, S_gemm);

    uint64_t dt = 0;

    for (int i = 0 ; i < N_ITER ; ++i)
    {
        int D = (MODE == 0) ? 0 : i * ld*(M+N);
        int L = D + ld*N;
        int U = D + N;
        int G = D + ld*N + N;

        uint64_t t0 = get_nanotime();
        // TODO
        uint64_t tf = get_nanotime();

        dt += (tf - t0);
    }

    double t = dt / (double)1e9;

    return 0;
}

# endif

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
        .name = "COPYSCALE",
        .f = main_copyscale,
        .nargs = 4,
        .descr = "???",
        .usage =    "M N BS_M BS_N\n"
                    "  - M      : number of rows of matrices L, and number of cols of U\n"
                    "  - N      : number of rows of matrices D and U, cols of matrices D and L\n"
                    "  - BS_M   : n° of (cols, rows) per tile of (L, U)\n"
                    "  - BS_N   : n° of (rows, cols) per tile of (L, U)\n"
    },
    {
        .name = "GEMM",
        .f = main_gemm,
        .nargs = 5,
        .descr = "C := A.B + C",
        .usage =    "M N K BS_M BS_N\n"
                    "  - M      : n° of rows of A and C\n"
                    "  - N      : n° of cols of B and C\n"
                    "  - K      : n° of cols of A, rows of B\n"
                    "  - BS_M   : n° of (cols, rows) per tile of (A, B)\n"
                    "  - BS_N   : n° of (rows, cols) per tile of (A, B)\n"
    },

    {
        .name = "TRSM",
        .f = main_trsm,
        .nargs = 4,
        .descr = "A.X = B",
        .usage =    "M N S\n"
                    "  - M      : number of rows of matrices L\n"
                    "  - N      : number of rows of matrices D, cols of matrices D and L\n"
                    "  - BS_M   : n° of (cols, rows) per tile of (D, L)\n"
                    "  - BS_N   : n° of (rows, cols) per tile of (L, D)\n"
    },

    {
        .name = "GEMM-GEMM",
        .f = main_gemm_gemm,
        .nargs = 7,
        .descr = "C := a.A.B + b.C - repeat twice with two block sizes",
        .usage =    "M N K\n"
                    "  - M      : n° of rows of A and C\n"
                    "  - N      : n° of cols of B and C\n"
                    "  - K      : n° of cols of A, rows of B\n"
                    "  - BS1_M  : 1st gemm block size\n"
                    "  - BS1_N  : 1st gemm block size\n"
                    "  - BS2_M  : 2nd gemm block size\n"
                    "  - BS2_N  : 2nd gemm block size\n"
    },


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

                printf("Init '%s'\n", impl.name());
                impl.init();
                printf("Running '%s'\n", func->name);
                int r = func->f(args);
                impl.deinit();
                return r;
            }
        }
    }

    error_usage(argv[0], NULL);
    return 1;
}
