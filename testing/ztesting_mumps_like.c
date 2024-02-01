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

int testing_zmumps_like(int argc, char **argv)
{
	// Operations: 
	// 	L = TRSM(L,D)
	//	U = L'
	//	L = L*D^{-1}
	//	G = L*U
	/* Check for number of arguments*/
	if( argc < 3 ) {
		USAGE("MUMPSLIKE", "m n ldd ldl ldu cpy",
			"\t- m   : number of rows of L, number of cols of U\n"
			"\t- n   : number of cols of L, number of rows and cols of D, number of rows of U\n"
			"\t- ld  : leading dimension of matrix D\n");
		return -1;
	}

	int M   = atoi(argv[0]);
	int N   = atoi(argv[1]);
	int LD = atoi(argv[2]);

	double eps;
	eps = LAPACKE_dlamch_work('e'); // TODO ???

	printf("\n");
	printf("------ TESTS FOR XKBLAS TRSM -> COPYSCALE -> GEMM  -------  \n");
	printf("            Size of the Matrix D %d by %d\n", N, N);
	printf("            Size of the Matrix L %d by %d\n", M, N);
	printf("            Size of the Matrix U %d by %d\n", N, M);
	printf("            Size of the Matrix G %d by %d\n", M, M);
	printf("\n");
	printf(" The matrices L and D are randomly generated for each test.\n");
	printf("============\n");
	printf(" The relative machine precision (eps) is to be %e \n",eps);

	/*----------------------------------------------------------
	 *  TESTING COPYSCALE
	 */

#define ITER 5
	for( int i_iter = 0; i_iter < ITER; i_iter++ )
	{
		/* Allocate */
		Complex64_t *A = (Complex64_t*) xkblas_malloc(LD*LD*sizeof(Complex64_t));
		Complex64_t *D = A;
		Complex64_t *U = A + N;
		Complex64_t *L = A + N * LD;
		Complex64_t *G = A + N * LD + N;
	
		if(!A)
		{
			xkblas_free(A,LD*LD*sizeof(Complex64_t));
			printf("Out of memory\n");
			return -2;
		}

		/* Prepare random data */
		LAPACKE_zlarnv_work(IONE, ISEED, LD*LD, A);
	
		/* Compute */
		Complex64_t one;
#if (PRECISION_s == 1) || (PRECISION_d == 1)
		one = 1;
#else
		one = 1;
		//one.x = 1;
		//one.y = 0;
#endif
		char side = 'L';
		char uplo = 'U';
		char transA = 'T';
		char diag = 'U';
		xkblas_ztrsm_async( xkblas_blas2cblas_side( &side ),
				xkblas_blas2cblas_fill( &uplo ),
				xkblas_blas2cblas_trans( &transA ),
				xkblas_blas2cblas_diag( &diag ),
				N, M, &one, D, LD, L, LD );	
				
		xkblas_zcopyscale_async( M, N, true, NULL, D, LD, L, LD, U, LD );
		
		printf("Before sync\n");
		xkblas_sync();
		printf("After sync\n");
		//xkblas_zgemmt_async( 


		/*
		xkblas_zcopyscale_async( M, N, CPY, NULL, D, LDD, L, LDL, U, LDU );
		xkblas_memory_coherent_async( 0, 0, M, N, Ufinal, LDU, sizeof(Complex64_t) );
		xkblas_memory_coherent_async( 0, 0, N, M, Lfinal, LDL, sizeof(Complex64_t) );
		xkblas_sync();
		xkblas_memory_invalidate_caches();
		*/

		xkblas_free(A,LD*LD*sizeof(Complex64_t));
	}

	printf("***************************************************\n");
	return 0;
}
