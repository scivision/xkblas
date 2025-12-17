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

# include <xkblas/auto-tile.h>
# include <xkblas/xkblas.hpp>
# include <xkblas/routine.hpp>
# include <xkblas/cblas.h>

# include <xkrt/support.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/utils/min-max.h>
# include <xkrt/memory/access/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>

# include <cassert>

XKRT_NAMESPACE_USE;

TYPED
struct args_t
{
    args_t(
        const int side, const int uplo,
        const int transA, const int diag,
        const size_t m, const size_t n,
        const TYPE alpha
    ) :
        side(side),
        uplo(uplo),
        transA(transA),
        diag(diag),
        m(m),
        n(n),
        alpha(alpha)
    {}

    ~args_t() {}

     const int side;
     const int uplo;
     const int transA;
     const int diag;
     const size_t m;
     const size_t n;
     const TYPE alpha;

};

TYPED
int
xkblas_t::trsm_tile_async(
    int side, int uplo,
    int transA, int diag,
    const size_t m, const size_t n,
    const TYPE * alpha,
    const TYPE * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda,
          TYPE * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb,
    device_global_id_t device_global_id
) {
    thread_t * thread = thread_t::get_tls();
    assert(thread);

    const size_t A_offset_m = Atm * Amb;
    const size_t A_offset_n = Atn * Anb;
    const size_t B_offset_m = Btm * Bmb;
    const size_t B_offset_n = Btn * Bnb;

    # define AC 2
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT | TASK_FLAG_DETACHABLE;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t<P>);

    task_t * task = thread->allocate_task(task_size + args_size);
    new (task) task_t(XKBLAS_XKRT_TASK_FORMAT_GET(P, TRSM), flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 1;
    new (dev) task_dev_info_t(device_global_id, ocr_access);

    args_t<P> * args = (args_t<P> *) TASK_ARGS(task, task_size);
    new (args) args_t<P>(side, uplo, transA, diag, m, n, *alpha);

    # ifndef XKRT_SUPPORT_DEBUG
    snprintf(task->label, sizeof(task->label),
            "trsm(A=(%zu,%zu) ; B=(%zu,%zu))",
            A_offset_m, A_offset_n, B_offset_m, B_offset_n);
    # endif /* XKRT_SUPPORT_DEBUG */

    /* TODO: block size, is that correct ? */
    const size_t Am = (side == CblasLeft) ? m : n;
    const size_t An = (side == CblasLeft) ? m : n;
    const size_t Bm = m;
    const size_t Bn = n;

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new (accesses + 0) access_t(task, MATRIX_COLMAJOR, A, lda, A_offset_m, A_offset_n, Am, An, sizeof(TYPE), ACCESS_MODE_R , ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    new (accesses + 1) access_t(task, MATRIX_COLMAJOR, B, ldb, B_offset_m, B_offset_n, Bm, Bn, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, ACCESS_SCOPE_NONUNIFIED);
    thread->resolve(accesses, AC);
    # undef AC

    this->runtime.task_commit(task);

    return 0;
}

TYPED
int
xkblas_t::trsm_async(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    if (m == 0 || n == 0)
        return 0;

    /* Check input arguments */
    if (side != CblasLeft && side != CblasRight)
    {
        LOGGER_ERROR("illegal value of side");
        return -1;
    }

    if ((uplo != CblasUpper) && (uplo != CblasLower))
    {
        LOGGER_ERROR("illegal value of uplo");
        return -2;
    }

    if (((transA < CblasNoTrans) || (transA > CblasConjTrans)))
    {
        LOGGER_ERROR("illegal value of transA");
        return -3;
    }

    if ((diag != CblasUnit) && (diag != CblasNonUnit))
    {
        LOGGER_ERROR("illegal value of diag");
        return -4;
    }

    if (m < 0)
    {
        LOGGER_ERROR("illegal value of m");
        return -5;
    }

    if (n < 0)
    {
        LOGGER_ERROR("illegal value of n");
        return -6;
    }

    const size_t Am = (side == CblasLeft) ? m : n;
    const size_t An = Am;
    const size_t Bm = m;
    const size_t Bn = n;

    if ((size_t) lda < MAX(1, An))
    {
        LOGGER_ERROR("illegal value of lda");
        return -8;
    }

    if ((size_t) ldb < MAX(1, Bn))
    {
        LOGGER_ERROR("illegal value of ldb");
        return -10;
    }

    size_t ts = this->conf.kernels[TRSM].tile;
    if (ts == 0)
    {
        int args[2] = {m, n};
        xkblas_routine_auto_tile(TRSM, args, &ts);
    }

    /* set tiling parameters */
    const size_t Amb = ts;
    const size_t Anb = ts;
    const size_t Bmb = ts;
    const size_t Bnb = ts;

    // const size_t Amt = NUM_OF_TILES(Am, Amb);
    // const size_t Ant = NUM_OF_TILES(An, Anb);
    const size_t Bmt = NUM_OF_TILES(Bm, Bmb);
    const size_t Bnt = NUM_OF_TILES(Bn, Bnb);

    /* distribute B in a cyclic-block manner */
    const int ngpus = this->runtime.drivers.devices.n - 1;
    distribution_t d;
    distribution2D_init(&d, XKRT_DISTRIBUTION_TYPE_CYCLIC2DBLOCK, ngpus, Bm, Bn, Bmb, Bnb);

    TYPE one        = (TYPE) 1.0;
    TYPE mone       = (TYPE)-1.0;
    TYPE minvalpha  = (TYPE)-1.0 / *alpha;

    # define A(I, J) A, (I), (J), Amb, Anb, lda
    # define B(I, J) B, (I), (J), Bmb, Bnb, ldb

    /* CblasLeft / CblasUpper / CblasNoTrans  */
    if (side == CblasLeft)
    {
        if (uplo == CblasUpper)
        {
            if (transA == CblasNoTrans)
            {
                for (size_t tk = 0; tk < Bmt; tk++)
                {
                    size_t bs_km  = (tk == 0) ? Bm-(Bmt-1)*Bmb : Bmb;
                    TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (size_t tn = 0; tn < Bnt; tn++)
                    {
                        const size_t Atm = Bmt-1-tk;
                        const size_t Atn = Bmt-1-tk;
                        const size_t Btm = Bmt-1-tk;
                        const size_t Btn = tn;
                        const device_global_id_t device_global_id = distribution2D_get(&d, Btm, Btn);

                        const size_t bs_nn = (Btn == Bnt-1) ? (Bn-Btn*Bnb) : Bnb;

                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(Atm, Atn),
                            B(Btm, Btn),
                            device_global_id
                        );
                    }
                    for (size_t tm = tk+1; tm < Bmt; ++tm)
                    {
                        for (size_t tn = 0; tn < Bnt; ++tn)
                        {
                            const device_global_id_t device_global_id = distribution2D_get(&d, Bmt-1-tm, tn);
                            const size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                CblasNoTrans, CblasNoTrans,
                                Bmb, bs_nn, bs_km,
                                &mone,
                                A(Bmt-1-tm, Bmt-1-tk),
                                B(Bmt-1-tk,       tn),
                                &lalpha,
                                B(Bmt-1-tm,       tn),
                                device_global_id
                            );
                        }
                    }
                }
            }
            /*
             *  CblasLeft / CblasUpper / CblasTrans
             */
            else
            {
                for (size_t tk = 0; tk < Bmt; ++tk)
                {
                    const size_t bs_km  = (tk == Bmt-1) ? Bm-tk*Bmb : Bmb;
                    const TYPE lalpha = (tk == 0)     ? *alpha : one;

                    for (size_t tn = 0; tn < Bnt; ++tn)
                    {
                        const size_t Atm = tk;
                        const size_t Atn = tk;
                        const size_t Btm = tk;
                        const size_t Btn = tn;
                        const device_global_id_t device_global_id = distribution2D_get(&d, Btm, Btn);

                        const size_t bs_nn = (Btn == Bnt-1) ? (Bn-Btn*Bnb) : Bnb;

                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(Atm, Atn),
                            B(Btm, Btn),
                            device_global_id
                        );
                    }
                    for (size_t tm = tk+1; tm < Bmt; tm++)
                    {
                        const size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (size_t tn = 0; tn < Bnt; ++tn)
                        {
                            const device_global_id_t device_global_id = distribution2D_get(&d, tm, tn);
                            const size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                transA, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                A(tk, tm),
                                B(tk, tn),
                                &lalpha,
                                B(tm, tn),
                                device_global_id
                            );
                        }
                    }
                }
            }
        }
        /*
         *  CblasLeft / CblasLower / CblasNoTrans
         */
        else
        {
            if (transA == CblasNoTrans)
            {
                for (size_t tk = 0; tk < Bmt; ++tk)
                {
                    const size_t bs_km  = (tk == Bmt-1) ? (Bm-tk*Bmb) : Bmb;
                    const TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (size_t tn = 0; tn < Bnt; ++tn)
                    {
                        const size_t Atm = tk;
                        const size_t Atn = tk;
                        const size_t Btm = tk;
                        const size_t Btn = tn;
                        const device_global_id_t device_global_id = distribution2D_get(&d, Btm, Btn);

                        const size_t bs_nn = (Btn == Bnt-1) ? (Bn-Btn*Bnb) : Bnb;

                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(Atm, Atn),
                            B(Btm, Btn),
                            device_global_id
                        );
                    }
                    for (size_t tm = tk+1; tm < Bmt; ++tm)
                    {
                        size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (size_t tn = 0; tn < Bnt; ++tn)
                        {
                            const device_global_id_t device_global_id = distribution2D_get(&d, tm, tn);
                            const size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                A(tm, tk),
                                B(tk, tn),
                                &lalpha,
                                B(tm, tn),
                                device_global_id
                            );
                        }
                    }
                }
            }
            /*
             *  CblasLeft / CblasLower / Cblas[Conj]Trans
             */
            else
            {
                for (size_t tk = 0; tk < Bmt; ++tk)
                {
                    const size_t bs_km  = (tk == 0) ? Bm-(Bmt-1)*Bmb : Bmb;
                    const TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (size_t tn = 0; tn < Bnt; ++tn)
                    {
                        const size_t Atm = Bmt-1-tk;
                        const size_t Atn = Bmt-1-tk;
                        const size_t Btm = Bmt-1-tk;
                        const size_t Btn = tn;
                        const device_global_id_t device_global_id = distribution2D_get(&d, Btm, Btn);

                        const size_t bs_nn = (Btn == Bnt-1) ? (Bn-Btn*Bnb) : Bnb;

                        this->trsm_tile_async<P>(
                            side, uplo, transA, diag,
                            bs_km, bs_nn,
                            &lalpha,
                            A(Atm, Atn),
                            B(Btm, Btn),
                            device_global_id
                        );
                    }
                    for (size_t tm = tk+1; tm < Bmt; ++tm)
                    {
                        for (size_t tn = 0; tn < Bnt; ++tn)
                        {
                            const size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            const device_global_id_t device_global_id = distribution2D_get(&d, Bmt-1-tm, tn);
                            this->gemm_tile_async<P>(
                                transA, CblasNoTrans,
                                Bmb, bs_nn, bs_km,
                                &mone,
                                A(Bmt-1-tk, Bmt-1-tm),
                                B(Bmt-1-tk,       tn),
                                &lalpha,
                                B(Bmt-1-tm,       tn),
                                device_global_id
                            );
                        }
                    }
                }
            }
        }
    }
    /*
     *  CblasRight / CblasUpper / CblasNoTrans
     */
    else
    {
        if (uplo == CblasUpper)
        {
            if (transA == CblasNoTrans)
            {
                for (size_t tk = 0; tk < Bnt; ++tk)
                {
                    const size_t bs_kn = (tk == Bnt-1) ? (Bn-tk*Bnb) : Bnb;
                    const TYPE lalpha = (tk == 0) ? *alpha : one;
                    for (size_t tm = 0; tm < Bmt; ++tm)
                    {
                        const size_t Atm = tk;
                        const size_t Atn = tk;
                        const size_t Btm = tm;
                        const size_t Btn = tk;
                        const device_global_id_t device_global_id = distribution2D_get(&d, Btm, Btn);

                        const size_t bs_mm = (Btm == Bmt-1) ? (Bm-Btm*Bmb) : Bmb;

                        this->trsm_tile_async<P>(
                            side, uplo, transA, diag,
                            bs_mm, bs_kn,
                            &lalpha,
                            A(Atm, Atn),
                            B(Btm, Btn),
                            device_global_id
                        );
                    }
                    for (size_t tm = 0; tm < Bmt; ++tm)
                    {
                        const size_t bs_mm = (tm == Bmt-1) ? (Bm-tm*Bmb) : Bmb;
                        for (size_t tn = tk+1; tn < Bnt; ++tn)
                        {
                            const device_global_id_t device_global_id = distribution2D_get(&d, tm, tn);
                            const size_t bs_nn = (tn == Bnt-1) ? (Bn-tn*Bnb) : Bnb;
                            this->gemm_tile_async<P>(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, bs_nn, Bmb,
                                &mone,
                                B(tm, tk),
                                A(tk, tn),
                                &lalpha,
                                B(tm, tn),
                                device_global_id
                            );
                        }
                    }
                }
            }
            /*
             *  CblasRight / CblasUpper / CblasConjTrans
             */
            else
            {
                for (size_t tk = 0; tk < Bnt; ++tk)
                {
                    const size_t bs_kn = tk == 0 ? Bn-(Bnt-1)*Bnb : Bnb;
                    for (size_t tm = 0; tm < Bmt; ++tm)
                    {
                        const size_t Atm = Bnt-1-tk;
                        const size_t Atn = Bnt-1-tk;
                        const size_t Btm = tm;
                        const size_t Btn = Bnt-1-tk;
                        const device_global_id_t device_global_id = distribution2D_get(&d, Btm, Btn);

                        const size_t bs_mm = (Btm == Bmt-1) ? (Bm-Btm*Bmb) : Bmb;

                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            alpha,
                            A(Atm, Atn),
                            B(Btm, Btn),
                            device_global_id
                        );

                        for (size_t tn = tk+1; tn < Bnt; ++tn)
                        {
                            this->gemm_tile_async<P>(
                                CblasNoTrans, transA,
                                bs_mm, Bnb, bs_kn,
                                &minvalpha,
                                B(Btm, Btn),
                                A(Bnt-1-tn, Bnt-1-tk),
                                &one,
                                B(tm, Bnt-1-tn),
                                device_global_id
                            );
                        }
                    }
                }
            }
        }
        /*
         *  CblasRight / CblasLower / CblasNoTrans
         */
        else
        {
            if (transA == CblasNoTrans)
            {
                for (size_t tk = 0; tk < Bnt; ++tk)
                {
                    const size_t bs_kn  = tk == 0 ? Bn-(Bnt-1)*Bnb : Bnb;
                    TYPE lalpha = tk == 0 ? *alpha : one;
                    for (size_t tm = 0; tm < Bmt; ++tm)
                    {
                        const size_t Atm = Bnt-1-tk;
                        const size_t Atn = Bnt-1-tk;
                        const size_t Btm = tm;
                        const size_t Btn = Bnt-1-tk;
                        const device_global_id_t device_global_id = distribution2D_get(&d, Btm, Btn);

                        const size_t bs_mm = (Btm == Bmt-1) ? (Bm-Btm*Bmb) : Bmb;

                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            &lalpha,
                            A(Atm, Atn),
                            B(Btm, Btn),
                            device_global_id
                        );

                        for (size_t tn = tk+1; tn < Bnt; ++tn)
                        {
                            this->gemm_tile_async<P>(
                                CblasNoTrans, CblasNoTrans,
                                bs_mm, Bnb, bs_kn,
                                &mone,
                                B(Btm, Btn),
                                A(Bnt-1-tk, Bnt-1-tn),
                                &lalpha,
                                B(tm, Bnt-1-tn),
                                device_global_id
                            );
                        }
                    }
                }
            }
            else
            {
                for (size_t tk = 0; tk < Bnt; ++tk)
                {
                    const size_t bs_kn = tk == Bnt-1 ? Bn-tk*Bnb : Bnb;
                    for (size_t tm = 0; tm < Bmt; ++tm)
                    {
                        const size_t Atm = tk;
                        const size_t Atn = tk;
                        const size_t Btm = tm;
                        const size_t Btn = tk;
                        const device_global_id_t device_global_id = distribution2D_get(&d, Btm, Btn);

                        const size_t bs_mm = (Btm == Bmt-1) ? (Bm-Btm*Bmb) : Bmb;

                        this->trsm_tile_async<P>(
                            side, uplo,
                            transA, diag,
                            bs_mm, bs_kn,
                            alpha,
                            A(Atm, Atn),
                            B(Btm, Btn),
                            device_global_id
                        );

                        for (size_t tn = tk+1; tn < Bnt; ++tn)
                        {
                            const size_t bs_nn = tn == Bnt-1 ? Bn-tn*Bnb : Bnb;
                            this->gemm_tile_async<P>(
                                CblasNoTrans, transA,
                                bs_mm, bs_nn, Bmb,
                                &minvalpha,
                                B(tm, tk),
                                A(tn, tk),
                                &one,
                                B(tm, tn),
                                device_global_id
                            );
                        }
                    }
                }
            }
        }
    }

    # undef A
    # undef B

    return 0;
}

TYPED
int
xkblas_t::trsm_sync(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    int r = this->trsm_async<P>(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb);
    this->sync();
    return r;
}

TYPED
int
xkblas_t::trsm(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb
) {
    this->memory_invalidate_caches();
    int r = this->trsm_async<P>(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb);
    this->memory_coherent_async(HOST_DEVICE_GLOBAL_ID, MATRIX_COLMAJOR, B, ldb, m, n, sizeof(TYPE));
    this->sync();
    return r;
}

TYPED
int
xkblas_t::trsm_rec_async(
    int side, int uplo,
    int transA, int diag,
    int m, int n,
    const TYPE * alpha,
    const TYPE * A, int lda,
          TYPE * B, int ldb,
    const int m_threshold
) {
    if (m <= m_threshold)
        return this->trsm_async<P>(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb);

    if (uplo == CblasUpper)
    {
        // From algorithm 1 of
        // Adaptive triangular system solving
        // Jean-Guillaume Dumas, Clément Pernet, Jean-Louis Roch

        //
        //     | A1  A2 |    | B1 |
        //     |     A3 |    | B2 |
        //

        // compute sub matrices
        const int m1 = m / 2;
        const int m2 = m - m1;
        const TYPE * A1 = A;
        const TYPE * A2 = (const TYPE *) matrix_tile_t::offset_addr(MATRIX_COLMAJOR, (const uintptr_t) A, lda, sizeof(TYPE),  0, m1);
        const TYPE * A3 = (const TYPE *) matrix_tile_t::offset_addr(MATRIX_COLMAJOR, (const uintptr_t) A, lda, sizeof(TYPE), m1, m1);
              TYPE * B1 = B;
              TYPE * B2 =       (TYPE *) matrix_tile_t::offset_addr(MATRIX_COLMAJOR, (const uintptr_t) B, ldb, sizeof(TYPE),  m1,  0);

        // TODO: if alpha != 1.0 i guess bellow is wrong
        assert(*alpha == (TYPE) 1.0);

        // lower part
        this->trsm_rec_async<P>(side, uplo, transA, diag, m2, n, alpha, A3, lda, B2, ldb, m_threshold);

        // middle part
        const int transB = CblasNoTrans;
        const TYPE  one = (const TYPE) +1.0;
        const TYPE mone = (const TYPE) -1.0;
        this->gemm_async<P>(transA, transB, m2, n, m2, &mone, A2, lda, B2, ldb, &one, B1, ldb);

        // upper part
        this->trsm_rec_async<P>(side, uplo, transA, diag, m1, n, alpha, A1, lda, B1, ldb, m_threshold);

    }
    else
    {
        assert(uplo == CblasLower);

        // From Fig. 1 of
        // Toward Portable GPU Performance: Julia Recursive Implementation of TRMM
        // and TRSM

        //
        //     | A11      |    | B1 |
        //     | A21  A22 |    | B2 |
        //

        // compute sub matrices
        const int m1 = m / 2;
        const int m2 = m - m1;
        const TYPE * A11 = A;
        const TYPE * A21 = (const TYPE *) matrix_tile_t::offset_addr(MATRIX_COLMAJOR, (const uintptr_t) A, lda, sizeof(TYPE),  m1, 0);
        const TYPE * A22 = (const TYPE *) matrix_tile_t::offset_addr(MATRIX_COLMAJOR, (const uintptr_t) A, lda, sizeof(TYPE), m1, m1);
              TYPE * B1 = B;
              TYPE * B2 = (TYPE *) matrix_tile_t::offset_addr(MATRIX_COLMAJOR, (const uintptr_t) B, ldb, sizeof(TYPE),  m1,  0);

        // TODO: if alpha != 1.0 i guess bellow is wrong
        assert(*alpha == (TYPE) 1.0);

        // upper part
        this->trsm_rec_async<P>(side, uplo, transA, diag, m1, n, alpha, A11, lda, B1, ldb, m_threshold);

        // middle part
        const int transB = CblasNoTrans;
        const TYPE  one = (const TYPE) +1.0;
        const TYPE mone = (const TYPE) -1.0;
        this->gemm_async<P>(transA, transB, m2, n, m2, &mone, A21, lda, B1, ldb, &one, B2, ldb);

        // lower part
        this->trsm_rec_async<P>(side, uplo, transA, diag, m2, n, alpha, A22, lda, B2, ldb, m_threshold);

    }

    return 0;
}

# if XKBLAS_SUPPORT_CUBLAS
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
static void
cuda_run(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    assert(queue);

    cublasHandle_t handle = queue->cu.blas.handle;
    assert(handle);

    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    XKBLAS_CUBLAS_CALL(
        FUNC(
            handle,
            cblas2cublas_side(args->side), cblas2cublas_uplo(args->uplo),
            cblas2cublas_op(args->transA), cblas2cublas_diag(args->diag),
            (int) args->m, (int) args->n,
            (const CU_TYPE *) &(args->alpha),
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
                  (CU_TYPE *) B->device_view.addr, (int) B->device_view.ld
        )
    );
}

TYPED
static void
cuda(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_cu_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_CUBLAS_DISPATCH_PRECISION(trsm);
}

# endif /* XKBLAS_SUPPORT_CUBLAS */


# if XKBLAS_SUPPORT_HIP
#  include <xkblas/hipblas-helper.h>
#  include <xkrt/driver/driver-hip.h>

template <xkblas_precision_t P, auto FUNC, typename CU_TYPE>
static void
hip_run(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_hip_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    assert(queue);

    hipblasHandle_t handle = queue->hip.blas.handle;
    assert(handle);

    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;

    assert(A->device_view.addr % A->host_view.sizeof_type == 0);
    assert(B->device_view.addr % B->host_view.sizeof_type == 0);

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    XKBLAS_HIPBLAS_CALL(
        FUNC(
            handle,
            cblas2hipblas_side(args->side), cblas2hipblas_uplo(args->uplo),
            cblas2hipblas_op(args->transA), cblas2hipblas_diag(args->diag),
            (int) args->m, (int) args->n,
            (const CU_TYPE *) &(args->alpha),
            (const CU_TYPE *) A->device_view.addr, (int) A->device_view.ld,
                  (CU_TYPE *) B->device_view.addr, (int) B->device_view.ld
        )
    );
}

TYPED
static void
hip(
    runtime_t * runtime,
    device_t * device,
    task_t * task,
    queue_hip_t * queue,
    command_t * cmd,
    queue_command_list_counter_t idx
) {
    XKBLAS_HIPBLAS_DISPATCH_PRECISION(trsm);
}

# endif /* XKBLAS_SUPPORT_HIP */


# if XKBLAS_SUPPORT_CBLAS

template <xkblas_precision_t P, auto FUNC>
static void
host_run(task_t * task)
{
    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * A = accesses + 0;
    const access_t * B = accesses + 1;

    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    assert(args);

    if constexpr (P == xkblas_precision_t::S || P == xkblas_precision_t::D)
    {
        FUNC(
            CblasColMajor,
            (const enum CBLAS_SIDE)      args->side,
            (const enum CBLAS_UPLO)      args->uplo,
            (const enum CBLAS_TRANSPOSE) args->transA,
            (const enum CBLAS_DIAG)      args->diag,
            (int) args->m, (int) args->n,
            (const TYPE  ) args->alpha,
            (const TYPE *) A->host_view.addr, (int) A->host_view.ld,
                  (TYPE *) B->host_view.addr, (int) B->host_view.ld
        );
    }
    else
    {
        FUNC(
            CblasColMajor,
            (const enum CBLAS_SIDE)      args->side,
            (const enum CBLAS_UPLO)      args->uplo,
            (const enum CBLAS_TRANSPOSE) args->transA,
            (const enum CBLAS_DIAG)      args->diag,
            (int) args->m, (int) args->n,
            (const TYPE *) &(args->alpha),
            (const TYPE *) A->host_view.addr, (int) A->host_view.ld,
                  (TYPE *) B->host_view.addr, (int) B->host_view.ld
        );
    }
}

TYPED
static void
host(task_t * task)
{
    if constexpr (P == xkblas_precision_t::S) host_run<P, cblas_strsm>(task);
    if constexpr (P == xkblas_precision_t::D) host_run<P, cblas_dtrsm>(task);
    if constexpr (P == xkblas_precision_t::C) host_run<P, cblas_ctrsm>(task);
    if constexpr (P == xkblas_precision_t::Z) host_run<P, cblas_ztrsm>(task);
}

# endif /* XKBLAS_SUPPORT_CBLAS */

# if 0
TYPED
static task_format_target_t
suggest_format(task_t * task)
{
    const args_t<P> * args = (const args_t<P> *) TASK_ARGS(task);
    if (args->m < 32)
        return XKRT_TASK_FORMAT_TARGET_HOST;
    return XKRT_TASK_FORMAT_TARGET_NO_SUGGEST;
}
# endif

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

# define ROUTINE_NAME TRSM

# define CL   0
# define CUDA 1
# define HIP  1
# define HOST 1
# define SYCL 0
# define ZE   0

# include "task-format.cc"

    # if 0
    format->suggest = (task_format_suggest_t) suggest_format<P>;
    # endif

# define DEFINE(P)  \
    template int xkblas_t::trsm<P>(int side, int uplo, int transA, int diag, int m, int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, xkblas_precision_type_t<P> * B, int ldb);    \
    template int xkblas_t::trsm_sync<P>(int side, int uplo, int transA, int diag, int m, int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, xkblas_precision_type_t<P> * B, int ldb);    \
    template int xkblas_t::trsm_async<P>(int side, int uplo, int transA, int diag, int m, int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, xkblas_precision_type_t<P> * B, int ldb);    \
    template int xkblas_t::trsm_rec_async<P>(int side, int uplo, int transA, int diag, int m, int n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, int lda, xkblas_precision_type_t<P> * B, int ldb, const int m_threshold);    \
    template int xkblas_t::trsm_tile_async<P>(int side, int uplo, int transA, int diag, const size_t m, const size_t n, const xkblas_precision_type_t<P> * alpha, const xkblas_precision_type_t<P> * A, const size_t Atm, const size_t Atn, const size_t Amb, const size_t Anb, const size_t lda, xkblas_precision_type_t<P> * B, const size_t Btm, const size_t Btn, const size_t Bmb, const size_t Bnb, const size_t ldb, device_global_id_t device_global_id);
XKBLAS_FORALL_PRECISIONS(DEFINE);
# undef DEFINE
