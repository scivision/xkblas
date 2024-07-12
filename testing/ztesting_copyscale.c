/**
 *
 * @file testing_zcopyscale.c
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2018 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief zcopyscale testing
 *
 * @version 1.0.0
 * @comment 
 * @author Mathieu Faverge
 * @author Emmanuel Agullo
 * @author Cedric Castagnede
 * @author Thierry Gautier, xkblas port
 * @author Pierre-Etienne Polet
 * @date 2024-01-26
 * @precisions normal z -> c d s
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "xkblas.h"
#include "ztask_internal.h"
#include "ztesting_auxiliary.h"

#include "flops.h"
void reference_copy2u_scalel( int M, int N, int CPY, int* IW, Complex64_t* D, int LDD, Complex64_t* L, int LDL, Complex64_t* U, int LDU );

int testing_zcopyscale(int argc, char **argv)
{
	/* Check for number of arguments*/
	if( argc < 6 ) {
		USAGE("COPYSCALE", "m n ldd ldl ldu cpy",
			"\t- m   : number of rows of L, number of cols of U\n"
			"\t- n   : number of cols of L, number of rows and cols of D, number of rows of U\n"
			"\t- ldd : leading dimension of matrix D\n"
			"\t- ldl : leading dimension of matrix L\n"
			"\t- ldu : leading dimension of matrix U\n"
			"\t- cpy : copy L' in U\n");
		return -1;
	}

	int M   = atoi(argv[0]);
	int N   = atoi(argv[1]);
	int LDD = atoi(argv[2]);
	int LDL = atoi(argv[3]);
	int LDU = atoi(argv[4]);
	bool CPY = atoi(argv[5]) == 1; 

	double eps;
	eps = LAPACKE_dlamch_work('e'); // TODO ???

	printf("\n");
	printf("------ TESTS FOR XKBLAS ZCOPYSCALE ROUTINE -------  \n");
	printf("            Size of the Matrix L %d by %d LD: %d\n", M, N, LDL);
	printf("            Size of the Matrix D %d by %d LD: %d\n", N, N, LDD);
	printf("            Size of the Matrix U %d by %d LD: %d\n", N, M, LDU);
	printf("\n");
	printf(" The matrices L and D are randomly generated for each test.\n");
	printf("============\n");
	printf(" The relative machine precision (eps) is to be %e \n",eps);
	printf(" Computational tests pass if scaled residuals are less than 10.\n");

	/*----------------------------------------------------------
	 *  TESTING COPYSCALE
	 */

#define ITER 1
	int suspicious = 0;
	double time[ITER];
	double flops[ITER];
	for( int i_iter = 0; i_iter < ITER; i_iter++ )
	{
		/* Allocate */
		Complex64_t *D = (Complex64_t*) xkblas_malloc(LDD*N*sizeof(Complex64_t));
		Complex64_t *L = (Complex64_t*) xkblas_malloc(LDL*M*sizeof(Complex64_t));
		Complex64_t *U = (Complex64_t*) xkblas_malloc(LDU*N*sizeof(Complex64_t));
		
		Complex64_t *Linit = (Complex64_t*) xkblas_malloc(LDL*M*sizeof(Complex64_t));
		Complex64_t *Uinit = (Complex64_t*) xkblas_malloc(LDU*N*sizeof(Complex64_t));
		Complex64_t *Lfinal = (Complex64_t*) xkblas_malloc(LDL*M*sizeof(Complex64_t));
		Complex64_t *Ufinal = (Complex64_t*) xkblas_malloc(LDU*N*sizeof(Complex64_t));
		
		
		if( (!D) || (!L) || (!U) || (!Linit) || (!Uinit) || (!Lfinal) || (!Ufinal) )
		{
			xkblas_free(D,LDD*N*sizeof(Complex64_t));
			xkblas_free(L,LDL*N*sizeof(Complex64_t));
			xkblas_free(U,LDU*M*sizeof(Complex64_t));
			xkblas_free(Linit,LDL*N*sizeof(Complex64_t));
			xkblas_free(Uinit,LDU*M*sizeof(Complex64_t));
			xkblas_free(Lfinal,LDL*N*sizeof(Complex64_t));
			xkblas_free(Ufinal,LDU*M*sizeof(Complex64_t));
			printf("Out of Memory\n");
			return -2;
		}

		/* Prepare random data */
		/*
#if (PRECISION_d == 1)
		for( int i = 0; i < LDD * N; i++ )
			D[i] = 2;
		for( int i = 0; i < LDL * M; i++ )
			L[i] = 1;
		for( int i = 0; i < LDU * N; i++ )
			U[i] = 3;
#endif
		printf("L:\n");
		for( int j = 0; j < M; j++ )
		{
		        for( int i = 0; i < N; i++ )
			{
				printf( "%.3lf ", L[i+j*LDL] );
			}
			printf("\n");
		}
		*/



		LAPACKE_zlarnv_work(IONE, ISEED, LDD*N, D);
		LAPACKE_zlarnv_work(IONE, ISEED, LDL*M, L);
		//memset( U, 0, sizeof(Complex64_t) * LDU * M );	
		
		LAPACKE_zlarnv_work(IONE, ISEED, LDU*N, U);
		
		#pragma omp parallel for
		for ( int i = 0; i < LDL*M; ++i) {
			Linit[i] = Lfinal[i] = L[i];
		}
		for ( int i = 0; i < LDU*N; ++i) {
			Uinit[i] = Ufinal[i] = U[i];
		}

#if (PRECISION_d == 1)
		printf("L[0] = %lf cpu at %p\n", L[0], L);
#endif

		/* Compute zcopyscale */
		double t0 = xkblas_elapsedtime();
		xkblas_zcopyscale_async( M, N, CPY, NULL, D, LDD, L, LDL, U, LDU );
		xkblas_memory_coherent_async( 0, 0, M, N, U, LDU, sizeof(Complex64_t) );
		xkblas_memory_coherent_async( 0, 0, N, M, L, LDL, sizeof(Complex64_t) );
		xkblas_sync();
		double t1 = xkblas_elapsedtime();
		xkblas_memory_invalidate_caches();
		sleep(2);

		// Apply copy2u scalel on the whole matrice
		reference_copy2u_scalel( M, N, CPY, NULL, D, LDD, Lfinal, LDL, Ufinal, LDU );

		// Compute flops
		time[i_iter] = t1-t0;
		flops[i_iter] = 0; //1e-9 * (...);

		/* Check the solution */
		if( getenv("NO_CHECK") == 0 ) {
		int u_valid = 0;

		/*
		printf("L init\n");
		for( int j = 0; j < M; j++ )
		{
		        for( int i = 0; i < N; i++ )
			{
				printf( "%.3lf ", Linit[i+j*LDL] );
			}
			printf("\n");
		}


		printf("U:\n");
		for( int i = 0; i < N; i++ )
		{
			for( int j = 0; j < M; j++ )
			{
				printf("%.3lf ", U[j+i*LDU]);
			}
			printf("\n");
		}
		*/

		for( int i = 0; i < N; i++ )
		{
			for( int j = 0; j < M; j++ )
			{
				if( Ufinal[j+i*LDU] != U[j+i*LDU] )
				{
#if (PRECISION_d==1)
					if( u_valid < 10 )
						printf("[%3d,%3d] %lf != %lf\n", i, j, Ufinal[j+i*LDU],  U[j+i*LDU] );
#endif
					u_valid++;
				}
			}
		}

		/*
		printf("Uref:\n");
		for( int i = 0; i < N; i++ )
		{
			for( int j = 0; j < M; j++ )
			{
				printf("%.3lf ", Ufinal[j+i*LDU]);
			}
			printf("\n");
		}
		*/

		if( u_valid > 0 )
			printf("\t U is invalid %d\n", u_valid);
			int info_solution = 0; // TODO implement check
			if (info_solution) 
				suspicious = 1;
		}
		
		xkblas_free(D,LDD*N*sizeof(Complex64_t));
		xkblas_free(L,LDL*N*sizeof(Complex64_t));
		xkblas_free(U,LDU*M*sizeof(Complex64_t));
		xkblas_free(Linit,LDL*N*sizeof(Complex64_t));
		xkblas_free(Uinit,LDU*M*sizeof(Complex64_t));
		xkblas_free(Lfinal,LDL*N*sizeof(Complex64_t));
		xkblas_free(Ufinal,LDU*M*sizeof(Complex64_t));
	}

	printf("***************************************************\n");
	if (suspicious == 0 )
	{

		printf("\t- TESTING ZCOPYSCALE ... PASSED !\n");
		// TODO do correct measure of execution time ?
		//for( int i = 0; i < ITER; i++ )
		//	printf("GFlops=%f\n", flops[i]/time[i]);
	}
	else
	{
		printf("\t- TESTING ZCOPYSCALE ... FAILED !\n");
		return 1;
	}
	return 0;
}

void reference_copy2u_scalel( int M, int N, int CPY, int* IW, Complex64_t* D, int LDD, Complex64_t* L, int LDL, Complex64_t* U, int LDU )
{
	for( int i = 0; i < N; i++ )
	{
		for( int j = 0; j < M; j++ )
		{
			// TODO deal with IW
			U[j + i * LDU] = L[i + LDL * j]; // Transpose
		}
	} 
}

static int xkblas_dscaling_cpu(
    int Irowmax, int Irowmin, int sizecopy,
    int LDA, int NCOLS, int LIW, int* IW, int offset_IW, int LA,
    double* A, int poselt, int A_Lpos, int A_Upos, int A_Dpos, bool copy_needed )
{
    int Lpos, Upos, Dpos;
    int posPV1, posPV2, OffDAG;
    int Irowend, Irow, Block2;

    int i,j;

    double mult1, mult2, A11, detPiv, A22, A12;

    int BLsizecopy;

    int LposI, UposI;
    bool pivot_2x2;

    if(sizecopy != 0){
        BLsizecopy = sizecopy;
    }

    else{
        BLsizecopy = 250;
    }

    for (Irowend = Irowmax; Irowend > Irowmin; Irowend -= BLsizecopy) {

        Block2 = min(BLsizecopy, Irowend);
        Irow = Irowend - Block2 + 1;
        Lpos = A_Lpos + (Irow-1) * LDA;
        Upos = A_Upos + (Irow-1);

        for (i = 1; i < NCOLS; i++) {

            pivot_2x2 = false;

            if (IW[offset_IW + i - 1] <= 0) {
                pivot_2x2 = true;
            }

            else {
               if (i > 1) {
                   if (IW[offset_IW + i - 2] <= 0) {
                        continue;
                   }
               }
            }

            Dpos = A_Dpos + (i-1) * LDA + i-1;

            //pivot 1x1

            if (!pivot_2x2) {

                A11 = 1.0 / A[Dpos];
                LposI = Lpos + i-1;

                if (copy_needed) {
                    UposI = Upos + (i-1) * LDA;

                    for (j = 1; j < Block2; j++){
                        A[UposI + (j-1)] = A[LposI + (j-1) * LDA];
                    }
                }

                for (j = 1; j < Block2; j++) {
                    A[LposI + (j-1) * LDA] *= A11;
                }
            }

            //pivot 2x2

            else {
                if (copy_needed){
                   cblas_dcopy(Block2, &A[Lpos + (i-1)], LDA, &A[Upos + (i-1)*LDA], 1);
                   cblas_dcopy(Block2, &A[Lpos + i], LDA, &A[Upos + i * LDA], 1);
                }

                posPV1 = Dpos;
                posPV2 = Dpos + LDA + 1;
                OffDAG = posPV1 + 1;

                A11 = A[posPV1];
                A22 = A[posPV2];
                A12 = A[OffDAG];
                detPiv = A11 * A22 - A12 * A12;

                A22 = A11 / detPiv;
                A11 = A[posPV2] / detPiv;
                A12 = - A12 / detPiv;

                for (j = 1; j < Block2; j++) {

                    mult1 = A11 * A[Lpos + i-1 + (j-1) * LDA] + A12 * A[Lpos + i + (j-1) * LDA];
                    mult2 = A12 * A[Lpos + i-1 + (j-1) * LDA] + A22 * A[Lpos + i + (j-1) * LDA];

                    if (copy_needed) {
                        A[Upos + j-1 + (i-1) * LDA] = A[Lpos + i-1 + (j-1) * LDA];
                        A[Upos + j-1 + i * LDA] = A[Lpos + i-1 + (j-1) * LDA];
                    }

                    A[Lpos + i-1 + (j-1) * LDA] = mult1;
                    A[Lpos + i + (j-1) * LDA] = mult2;
                }
            }
        }
    }

    return 0;
}


