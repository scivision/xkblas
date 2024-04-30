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
	printf("            Size of the Matrix L %d by %d\n", M, N);
	printf("\n");
	printf(" The matrices L and D are randomly generated for each test.\n");
	printf("============\n");
	printf(" The relative machine precision (eps) is to be %e \n",eps);
	printf(" Computational tests pass if scaled residuals are less than 10.\n");

	/*----------------------------------------------------------
	 *  TESTING COPYSCALE
	 */

#define ITER 5
	int suspicious = 0;
	double time[ITER];
	double flops[ITER];
	for( int i_iter = 0; i_iter < ITER; i_iter++ )
	{
		/* Allocate */
		Complex64_t *D = (Complex64_t*) xkblas_malloc(LDD*N*sizeof(Complex64_t));
		Complex64_t *L = (Complex64_t*) xkblas_malloc(LDL*N*sizeof(Complex64_t));
		Complex64_t *U = (Complex64_t*) xkblas_malloc(LDU*M*sizeof(Complex64_t));
		
		Complex64_t *Linit = (Complex64_t*) xkblas_malloc(LDL*N*sizeof(Complex64_t));
		Complex64_t *Uinit = (Complex64_t*) xkblas_malloc(LDU*M*sizeof(Complex64_t));
		Complex64_t *Lfinal = (Complex64_t*) xkblas_malloc(LDL*N*sizeof(Complex64_t));
		Complex64_t *Ufinal = (Complex64_t*) xkblas_malloc(LDU*M*sizeof(Complex64_t));
		
		
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
		LAPACKE_zlarnv_work(IONE, ISEED, LDD*N, D);
		LAPACKE_zlarnv_work(IONE, ISEED, LDL*N, L);
		LAPACKE_zlarnv_work(IONE, ISEED, LDU*M, U);
		
		#pragma omp parallel for
		for ( int i = 0; i < LDL*N; ++i) {
			Linit[i] = Lfinal[i] = L[i];
		}
		for ( int i = 0; i < LDU*M; ++i) {
			Uinit[i] = Ufinal[i] = U[i];
		}

		/* Compute zcopyscale */
		double t0 = xkblas_elapsedtime();
		xkblas_zcopyscale_async( M, N, CPY, NULL, D, LDD, L, LDL, U, LDU );
		xkblas_memory_coherent_async( 0, 0, M, N, U, LDU, sizeof(Complex64_t) );
		xkblas_memory_coherent_async( 0, 0, N, M, L, LDL, sizeof(Complex64_t) );
		xkblas_sync();
		double t1 = xkblas_elapsedtime();
		xkblas_memory_invalidate_caches();

		// Compute flops
		time[i_iter] = t1-t0;
		flops[i_iter] = 0; //1e-9 * (...);

		/* Check the solution */
		if( getenv("NO_CHECK") == 0 ) {
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
