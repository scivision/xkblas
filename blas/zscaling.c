/**
 *
 * @file zgemm.c
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2018 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief Chameleon zgemm wrappers
 *
 * @version 1.0.0
 * @comment This file has been automatically generated
 *          from Plasma 2.5.0 for CHAMELEON 1.0.0
 * @author Mathieu Faverge
 * @author Emmanuel Agullo
 * @author Cedric Castagnede
 * @author Thierry Gautier, add zgemmt
 * @author Pierre-Etienne Polet
 * @date 2024-01-24
 * @precisions normal z -> s d c
 * This file was merged from pzgemm and zgemm from Chameleon by Thierry Gautier
 * for Kaapi that support natively 2D memory view.
 */
#include "common.h"
#include "ztask.h"
#include "ztask_internal.h"
#include <string.h>

#ifdef KAAPI_DEBUG
#undef KAAPI_DEBUG
#endif
//#define KAAPI_DEBUG 1

// TODO redefine this ...
/**
 ********************************************************************************
 *
 * @ingroup Complex64_t
 *
 *  zgemmt - Performs one of the matrix-matrix operations
 *
 *    \f[ C = \alpha [op( A )\times op( B )] + \beta C \f],
 *
 *  where op( X ) is one of
 *
 *    op( X ) = X  or op( X ) = X' or op( X ) = conjg( X' )
 *
 *  alpha and beta are scalars, and A, B and C  are matrices, with op( A )
 *  an m by k matrix, op( B ) a k by n matrix and C an m by n matrix.
 *
 *******************************************************************************
 *
 * @param[in] transA
 *          Specifies whether the matrix A is transposed, not transposed or conjugate transposed:
 *          = CblasNoTrans:   A is not transposed;
 *          = CblasTrans:     A is transposed;
 *          = CblasConjTrans: A is conjugate transposed.
 *
 * @param[in] transB
 *          Specifies whether the matrix B is transposed, not transposed or conjugate transposed:
 *          = CblasNoTrans:   B is not transposed;
 *          = CblasTrans:     B is transposed;
 *          = CblasConjTrans: B is conjugate transposed.
 *
 * @param[in] N
 *          N specifies the number of columns of the matrix op( B ) and of the matrix C. N >= 0.
 *          N specifies the number of rows of the matrix op( A ) and of the matrix C. N >= 0.
 *
 * @param[in] K
 *          K specifies the number of columns of the matrix op( A ) and the number of rows of
 *          the matrix op( B ). K >= 0.
 *
 * @param[in] alpha
 *          alpha specifies the scalar alpha
 *
 * @param[in] A
 *          A is a LDA-by-ka matrix, where ka is K when  transA = CblasNoTrans,
 *          and is  M  otherwise.
 *
 * @param[in] LDA
 *          The leading dimension of the array A. LDA >= max(1,M).
 *
 * @param[in] B
 *          B is a LDB-by-kb matrix, where kb is N when  transB = CblasNoTrans,
 *          and is  K  otherwise.
 *
 * @param[in] LDB
 *          The leading dimension of the array B. LDB >= max(1,N).
 *
 * @param[in] beta
 *          beta specifies the scalar beta
 *
 * @param[in,out] C
 *          C is a LDC-by-N matrix.
 *          On exit, the array is overwritten by the M by N matrix ( alpha*op( A )*op( B ) + beta*C )
 *
 * @param[in] LDC
 *          The leading dimension of the array C. LDC >= max(1,M).
 *
 *******************************************************************************
 *
 * @return
 *          \retval 0 successful exit
 *
 *******************************************************************************
 *
 * @sa CHAMELEON_zgemm_Tile
 * @sa CHAMELEON_cgemm
 * @sa CHAMELEON_dgemm
 * @sa CHAMELEON_sgemm
 *
 */

#define D(m, n) D##h,  m,  n
#define L(m, n) L##h,  m,  n
#define U(m, n) U##h,  m,  n

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

int xkblas_zscaling_async(
	/*
	
	int i_rowmax, int i_rowmin, int sizecopy, 
	int lda, int n_cols, 
	int lIW, int* IW, int offset_IW,
	int la, Complex64_t* A, 
	size_t pos_elt, size_t A_Lpos, size_t A_Upos, size_t A_Dpos, bool copy_needed)
{
	// unused variables sizecopy, lIW, pos_elt
	int M = i_rowmax - i_rowmin;
	int N = n_cols;
	Complex64_t* D = A + A_Dpos;
	Complex64_t* L = A + A_Lpos;
	Complex64_t* U = A + A_Upos;
	// TODO IW handling apply offset
	int ldd = lda;
	int ldl = lda;
	int ldu = lda;
	*/
	size_t M, size_t N, bool should_copy,
	//int* IW,
	Complex64_t* D, size_t ldd,
	Complex64_t* L, size_t ldl,
	Complex64_t* U, size_t ldu)
{
	// TODO Check input args
	if( ldd < N )
	{
		kaapi_error("zscaling", "Invalid value for ldd\n");
		return -5;
	}
	if( ldl < N )
	{
		kaapi_error("zscaling", "Invalid value for ldl\n");
		return -7;
	}
	if( ldu < M )
	{
		kaapi_error("zscaling", "Invalid value for ldu\n");
		return -9;
	}
	
	// TODO check if something to do ... n_cols > 0 
		
	// get default tile size and initialize internal descriptor if not yet
	size_t NB = xkblas_auto_tilesize(KERN_SCALING,M,N,N); // TODO define the case for KERN_SCALING

       	// TODO add something to force synchronous call ?
	
	xkblas_matrix_descr_t* Dh = xkblas_find(D);	
	xkblas_matrix_descr_t* Lh = xkblas_find(L);	
	xkblas_matrix_descr_t* Uh = xkblas_find(U);	
	// TODO IW handle add desc
	
    	if (!xkblas_matrix_descr_isinit(Dh))
      		xkblas_init_matrix_handle(Dh, (void*)D, N, N, ldd, sizeof(Complex64_t), NB, NB);
      	kaapi_assert_debug( (Dh->ld == ldd) && (Dh->M == N) && (Dh->N == N) );
    	if (!xkblas_matrix_descr_isinit(Lh))
      		xkblas_init_matrix_handle(Lh, (void*)L, M, N, ldl, sizeof(Complex64_t), NB, NB);
      	kaapi_assert_debug( (Lh->ld == ldl) && (Lh->M == M) && (Lh->N == N) );
    	if (!xkblas_matrix_descr_isinit(Uh))
      		xkblas_init_matrix_handle(Uh, (void*)U, N, M, ldu, sizeof(Complex64_t), NB, NB);
      	kaapi_assert_debug( (Uh->ld == ldu) && (Uh->M == N) && (Uh->N == M) );

#if defined(KAAPI_DEBUG)
	{
		assert( 0 == xkblas_dbg_setname( "D", Dh ) );
		assert( 0 == xkblas_dbg_setname( "L", Lh ) );
		assert( 0 == xkblas_dbg_setname( "U", Uh ) );
	}
#endif
	/* map output of A on ressources */
	xkblas_context_t* xkctxt = xkblas_context_get();
	xkblas_auto_map( xkctxt, KERN_SCALING, Lh );

	// TODO implement trace search KAAPI_USE_TRACELIB==1

	for( int blockIdx_m = 0; blockIdx_m < Lh->mt; blockIdx_m++ )
	{
		int m = MIN(M - blockIdx_m * Lh->mb, Lh->mb);
		for( int blockIdx_n = 0; blockIdx_n < Lh->nt; blockIdx_n++ )
		{
			int n = MIN(N - blockIdx_n * Lh->nb, Lh->nb);

			INSERT_TASK_zscaling(m,n,should_copy,
			//	NULL, 0, 0,
				D(n,n), ldd, 
				L(m,n), ldl, 
				U(n,m), ldu );

		}
	}
	return 0;
}

/* CPU driver */
extern int xkblas_zscaling_native(
	size_t m, size_t n, bool should_copy,
	//int* IW,
	Complex64_t* D, size_t ldd,
	Complex64_t* L, size_t ldl,
	Complex64_t* U, size_t ldu)
{
	// TODO need to check validity ??
	int bsizecopy = 250;	
	for( int i_row_start = 0; i_row_start < m; i_row_start += bsizecopy )
	{
		int blocksize = MIN(bsizecopy, m - i_row_start * bsizecopy);
		for( int i_col = 0; i_col < n; i_col++ )
		{
			// TODO implement 2x2 case (if needed)
			Complex64_t A11 = 1.0/D[i_col + i_col * ldd];
			if( should_copy )
			{
				for( int i_row = 0; i_row < blocksize; i_row++ )
				{
					U[ (i_row_start+i_row) + i_col * ldu  ] = L[ (i_row_start+i_row) * ldl + i_col ];
				}
			}	
			for( int i_row = 0; i_row < blocksize; i_row++ )
			{
				L[ (i_row_start+i_row) * ldl + i_col ] *= A11;
			}

		}
	}	

	return 0;
}
