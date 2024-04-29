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

void check_d_matrice( double* A, double* B, int m, int n, int ld ){
	int error_count = 0;
	for( int i = 0; i < m; i++ )
	{
		for( int j = 0; j < n; j++ )
		{
		  //printf("%.2f ", A[i + j*ld]);
			if( A[i + j*ld] != B[i + j*ld] )
				error_count++;
		}
		//printf("\n");
	}
	printf("Error count = %d\n", error_count);
}

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

#define ITER 1
	for( int i_iter = 0; i_iter < ITER; i_iter++ )
	{
		/* Allocate */
		Complex64_t *A = (Complex64_t*) xkblas_malloc(LD*LD*sizeof(Complex64_t));
		Complex64_t *D = A;
		Complex64_t *U = A + N;
		Complex64_t *L = A + N * LD;
		Complex64_t *G = A + N * LD + N;
		printf("Allocated: %p -> +0x%x\n", A, LD*LD*sizeof(Complex64_t));

		Complex64_t *A_ref = (Complex64_t*) xkblas_malloc(LD*LD*sizeof(Complex64_t));
		Complex64_t *D_ref = A_ref;
		Complex64_t *U_ref = A_ref + N;
		Complex64_t *L_ref = A_ref + N * LD;
		Complex64_t *G_ref = A_ref + N * LD + N;
	
		if(!A)
		{
			xkblas_free(A,LD*LD*sizeof(Complex64_t));
			printf("Out of memory\n");
			return -2;
		}
		if(!A_ref)
		{
			xkblas_free(A_ref,LD*LD*sizeof(Complex64_t));
			printf("Out of memory\n");
			return -2;
		}

		/* Prepare random data */
		long long int seeds[4] = {1, 2, 3, 4};
		int info = LAPACKE_zlarnv_work(1, seeds, LD*LD, A);
		memcpy( A_ref, A, LD*LD*sizeof(Complex64_t) );

		/* Compute */
		Complex64_t one = 1;
		Complex64_t zero = 0;
		char side = 'L';
		char uplo = 'U';
		char transA = 'T';
		char diag = 'U';
		char transL = 'T';
		char transU = 'T';	
		
		// Execute version without sync
		xkblas_ztrsm_async( xkblas_blas2cblas_side( &side ), xkblas_blas2cblas_fill( &uplo ), xkblas_blas2cblas_trans( &transA ), xkblas_blas2cblas_diag( &diag ),
				N, M, &one, D, LD, L, LD );
		//xkblas_sync();

		xkblas_zcopyscale_async( M, N, true, NULL, D, LD, L, LD, U, LD );
		//xkblas_sync();

		xkblas_zgemm_async( xkblas_blas2cblas_trans(&transL), xkblas_blas2cblas_trans(&transU),
				M, M, N, &one, L, LD, U, LD, &one, G, LD );
		//xkblas_sync();
		
		xkblas_memory_coherent_async( 0, 0, N, M, L, LD, sizeof(Complex64_t)); // Get L back
		xkblas_memory_coherent_async( 0, 0, M, N, U, LD, sizeof(Complex64_t)); // Get U back
		xkblas_memory_coherent_async( 0, 0, M, M, G, LD, sizeof(Complex64_t)); // Get G back

		xkblas_sync();
		xkblas_memory_invalidate_caches();
		printf("Without sync execution done\n");

		// Execute version with sync
		xkblas_ztrsm_async( xkblas_blas2cblas_side( &side ), xkblas_blas2cblas_fill( &uplo ), xkblas_blas2cblas_trans( &transA ), xkblas_blas2cblas_diag( &diag ),
				N, M, &one, D_ref, LD, L_ref, LD );
		xkblas_sync();

		xkblas_zcopyscale_async( M, N, true, NULL, D_ref, LD, L_ref, LD, U_ref, LD );
		xkblas_sync();

		xkblas_zgemm_async( xkblas_blas2cblas_trans(&transL), xkblas_blas2cblas_trans(&transU),
				M, M, N, &one, L_ref, LD, U_ref, LD, &one, G_ref, LD );
		
		xkblas_memory_coherent_async( 0, 0, N, M, L_ref, LD, sizeof(Complex64_t)); // Get L back
		xkblas_memory_coherent_async( 0, 0, M, N, U_ref, LD, sizeof(Complex64_t)); // Get U back
		xkblas_memory_coherent_async( 0, 0, M, M, G_ref, LD, sizeof(Complex64_t)); // Get G back
		xkblas_sync();
		xkblas_memory_invalidate_caches();
		printf("With sync execution done\n");

#if (PRECISION_d == 1)
		check_d_matrice( A, A_ref, LD, LD, LD );
#endif

		xkblas_free(A,LD*LD*sizeof(Complex64_t));
		xkblas_free(A_ref,LD*LD*sizeof(Complex64_t));
	}

	printf("***************************************************\n");
	return 0;
}
