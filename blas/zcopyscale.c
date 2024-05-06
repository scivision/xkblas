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

int xkblas_zcopyscale_async(
	size_t M, size_t N, bool should_copy,
	int* IW,
	Complex64_t* D, size_t ldd,
	Complex64_t* L, size_t ldl,
	Complex64_t* U, size_t ldu)
{
	printf("[XKBLAS] copyscale M %d, N %d, D %p, ldd %d, L %p, ldl %d, U %p, ldu %d\n",
									M, N, D, ldd, L, ldl, U, ldu );
	// TODO Check input args
	if( ldd < N )
	{
		kaapi_error("zcopyscale", "Invalid value for ldd\n");
		return -5;
	}
	if( ldl < N )
	{
		kaapi_error("zcopyscale", "Invalid value for ldl\n");
		return -7;
	}
	if( ldu < M )
	{
		kaapi_error("zcopyscale", "Invalid value for ldu\n");
		return -9;
	}
	
	// TODO check if something to do ... n_cols > 0 
		
	// get default tile size and initialize internal descriptor if not yet	
	xkblas_matrix_descr_t* Dh = xkblas_find(D);	
	xkblas_matrix_descr_t* Lh = xkblas_find(L);
	xkblas_matrix_descr_t* Uh = xkblas_find(U);
	// TODO IW handle add desc

	int Dstatus = xkblas_matrix_descr_isinit(Dh);
	int Lstatus = xkblas_matrix_descr_isinit(Lh);
	int Ustatus = xkblas_matrix_descr_isinit(Uh);

	int n_block = 0;
	int m_block = 0;
	{
		// Try to define common NB and MB size
		if( Dstatus )
		{
			kaapi_assert_debug_m( Dh->nb == Dh->mb, "copyscale error: unsupported D splitting" );
			n_block = Dh->nb;
		}

		if( Lstatus )
		{
			if( n_block == 0 )
							n_block = Lh->mb;
			m_block = Lh->nb;
			kaapi_assert_debug_m( n_block == Lh->mb, "copyscale error: L and D splitting are incompatible" );
		}

		if( Ustatus )
		{
			if( m_block == 0 )
				m_block = Uh->mb;
			if( n_block == 0 )
				n_block = Uh->nb;
			kaapi_assert_debug_m( (n_block == Uh->nb) && (m_block == Uh->mb), "copyscale error: U splitting incompatible with L and D"  );
		}

		size_t NB = xkblas_auto_tilesize(KERN_COPYSCALE,M,N,N); // TODO define the case for KERN_COPYSCALE
		if( n_block == 0 )
						n_block = NB;
		if( m_block == 0 )
						m_block = NB;

		printf("[XKBLAS] cpyscale mb %d, nb %d, M %d, N %d\n", m_block, n_block, M, N);
		// init unitialised matrices
		if( !Dstatus )
			xkblas_init_matrix_handle(Dh, (void*)D, N, N, ldd, sizeof(Complex64_t), n_block, n_block);
		if( !Lstatus )
			xkblas_init_matrix_handle(Lh, (void*)L, N, M, ldl, sizeof(Complex64_t), n_block, m_block);
		if( !Ustatus )
			xkblas_init_matrix_handle(Uh, (void*)U, M, N, ldu, sizeof(Complex64_t), m_block, n_block);
	}
	kaapi_assert_debug_m( (Dh->ld == ldd) && (Dh->M == N) && (Dh->N == N), "Invalid matrice D" );
	kaapi_assert_debug_m( (Lh->ld == ldl) && (Lh->M == N) && (Lh->N == M), "Invalid matrice L" );
	kaapi_assert_debug_m( (Uh->ld == ldu) && (Uh->M == M) && (Uh->N == N), "Invalid matrice U" );

#if defined(KAAPI_DEBUG)
	{
		kaapi_assert( 0 == xkblas_dbg_setname( "D", Dh ) );
		kaapi_assert( 0 == xkblas_dbg_setname( "L", Lh ) );
		kaapi_assert( 0 == xkblas_dbg_setname( "U", Uh ) );
	}
#endif

	/* map output of A on ressources */
	xkblas_context_t* xkctxt = xkblas_context_get();
	xkblas_auto_map( xkctxt, KERN_COPYSCALE, Lh );

#if KAAPI_USE_TRACELIB==1
	kaapi_context_t* ctxt = xkctxt->kctxt;
	kaapi_event_t* evt = KAAPI_EVENT_GET(&ctxt->kproc, KAAPI_EVT_CALL, 0 );
	if(evt)
	{
		strncpy(evt->u.s.d0.c8,"zcpys",8);
		evt->u.s.d1.u = M;
		evt->u.s.d2.u = N;
		KAAPI_EVENT_PUSH(&ctxt->kproc, KAAPI_EVT_CALL);
	}
#endif

	for( int blockIdx_m = 0; blockIdx_m < Lh->nt; blockIdx_m++ )
	{
		int m = MIN(M - blockIdx_m * Lh->nb, Lh->nb);
		for( int blockIdx_n = 0; blockIdx_n < Lh->mt; blockIdx_n++ )
		{
			int n = MIN(N - blockIdx_n * Lh->mb, Lh->mb);
			INSERT_TASK_zcopyscale(m,n,should_copy,
			//	NULL, 0, 0,
				D(blockIdx_n, blockIdx_n), ldd,
				L(blockIdx_n, blockIdx_m), ldl,
				U(blockIdx_m, blockIdx_n), ldu );
			//	D(n,n), ldd, 
			//	L(m,n), ldl, 
			//	U(n,m), ldu );

		}
	}
	return 0;
}

/* CPU driver */
extern int xkblas_zcopyscale_native(
	size_t m, size_t n, bool should_copy,
	int* IW,
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
