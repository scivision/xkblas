# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <string.h>
# include <time.h>

# include "blas.h"
# include "common/s.h"
# include "common/impl.hpp"

static impl_t impl;

uint64_t
get_nanotime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000000) + (uint64_t) ts.tv_nsec;
}

/**
 *  GEMM(M, N, D, L, U)
 */
static int
main_gemm(char ** args)
{
    int transA = CblasNoTrans;
    int transB = CblasNoTrans;
    int M = atoi(args[0]);
    int N = atoi(args[1]);
    int K = atoi(args[2]);
    int S1 = atoi(args[3]);
    int S2 = atoi(args[4]);
    const TYPE alpha = (const TYPE) 1.0;
    const TYPE beta  = (const TYPE) 1.0;
    int LD = M+N+K;

    uintptr_t mem = (uintptr_t) malloc(LD + 3 * sizeof(TYPE) * (LD * LD));
    uintptr_t Ap  = mem + (LD - (mem % LD)) + 0 * sizeof(TYPE) * (LD * LD);
    uintptr_t Bp  = mem + (LD - (mem % LD)) + 1 * sizeof(TYPE) * (LD * LD);
    uintptr_t Cp  = mem + (LD - (mem % LD)) + 2 * sizeof(TYPE) * (LD * LD);

    assert(Ap % LD == 0);
    assert(Bp % LD == 0);
    assert(Cp % LD == 0);

    printf("Set (M, N, K) = (%d, %d, %d) with tile (%d, %d)\n", M, N, K, S1, S2);

    uint64_t t0 = get_nanotime();
    // TODO
    uint64_t tf = get_nanotime();

    printf("Took %lf s.\n", (tf - t0) / (double)1e9);

    return 0;
}

/**
 *  TRSM(M, N, D, L, S)
 */
static int
main_trsm(char ** args)
{
    int M  = atoi(args[0]);
    int N  = atoi(args[1]);
    int LD = N + M;
    int S = atoi(args[2]);

    if (LD < M || LD < N)
    {
        fprintf(stderr, "LD must be greated than M and N\n");
        return 1;
    }
    if (M % S || N % S)
    {
        fprintf(stderr, "Tile sizes must divide matrix sizes");
        return 1;
    }

    int D = 0;
    int L = LD * N;

    printf("Set (M, N, LD) = (%d, %d, %d) with tile (%d, %d)\n", M, N, LD, S, S);

    uint64_t t0 = get_nanotime();
    // TODO
    uint64_t tf = get_nanotime();

    printf("Took %lf s.\n", (tf - t0) / (double)1e9);

    return 0;
}

/**
 *  COPYSCALE(M, N, D, L, U)
 */
static int
main_copyscale(char ** args)
{
    int M  = atoi(args[0]);
    int N  = atoi(args[1]);
    int LD = N + M;
    int S = atoi(args[2]);

    if (LD < M || LD < N)
    {
        fprintf(stderr, "LD must be greated than M and N\n");
        return 1;
    }
    if (M % S || N % S)
    {
        fprintf(stderr, "Tile sizes must divide matrix sizes");
        return 1;
    }

    int D = 0;
    int L = LD * N;
    int U = N;

    printf("Set (M, N, LD) = (%d, %d, %d) with tile (%d, %d)\n", M, N, LD, S, S);

    uint64_t t0 = get_nanotime();
    // TODO
    uint64_t tf = get_nanotime();

    printf("Took %lf s.\n", (tf - t0) / (double)1e9);

    return 0;
}

/**
 *  TRSM(_, N, M, LD, LD)
 *  COPYSCALE(M, N, LD, LD, LD, 1)
 *  GEMM(_, _, M, M, N, LD, LD, LD)
 */
static int
main_trsm_copyscale_gemm(char ** args)
{
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

    int LD = N + M;
    printf("Set (M, N, LD) = (%d, %d, %d) with tiles (S, trsm, copyscale, gemm)=(%d, %d, %d, %d)\n", M, N, LD, S, S_trsm, S_copyscale, S_gemm);

    uint64_t dt = 0;

    for (int i = 0 ; i < N_ITER ; ++i)
    {
        int D = (MODE == 0) ? 0 : i * LD*(M+N);
        int L = D + LD*N;
        int U = D + N;
        int G = D + LD*N + N;

        uint64_t t0 = get_nanotime();
        // TODO
        uint64_t tf = get_nanotime();

        dt += (tf - t0);
    }

    double t = dt / (double)1e9;

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

    {
        .name = "COPYSCALE",
        .f = main_copyscale,
        .nargs = 3,
        .usage =    "M N S\n"
                    "  - M      : number of rows of matrices L, and number of cols of U\n"
                    "  - N      : number of rows of matrices D and U, cols of matrices D and L\n"
                    "  - S      : number of rows and cols per tile\n"
    },

    {
        .name = "GEMM",
        .f = main_gemm,
        .nargs = 5,
        .descr = "C := A.B + C",
        .usage =    "M N K S1 S2\n"
                    "  - M      : n° of rows of A and C\n"
                    "  - N      : n° of cols of B and C\n"
                    "  - K      : n° of cols of A, rows of B\n"
                    "  - S1     : n° of rows per tile\n"
                    "  - S2     : n° of cols per tile\n"
    },

    {
        .name = "TRSM",
        .f = main_trsm,
        .nargs = 3,
        .usage =    "M N S\n"
                    "  - M      : number of rows of matrices L\n"
                    "  - N      : number of rows of matrices D, cols of matrices D and L\n"
                    "  - S      : number of rows and cols per tile\n"
    },
};
# define N_FUNCS (sizeof(funcs) / sizeof(func_t))

static int
error_usage(const char * label, func_t * func)
{
    if (func == NULL)
    {
        fprintf(stderr, "usage : %s [FUNC] [...]\n", label);
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
                if (nargs < func->nargs)
                    return error_usage(argv[0], func);

                // TODO
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
