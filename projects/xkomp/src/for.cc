# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

extern "C"
void
__kmpc_for_static_fini(
    ident_t * loc,
    kmp_int32 global_tid
) {
}

extern "C"
void
__kmpc_for_static_init_4(
    ident_t *loc,
    kmp_int32 gtid,
    kmp_int32 schedtype,
    kmp_int32 *plastiter,
    kmp_int32 *plower,
    kmp_int32 *pupper,
    kmp_int32 *pstride,
    kmp_int32 incr,
    kmp_int32 chunk
) {
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    kmp_int32 pupper_old = *pupper;
	kmp_int32 trip_count = (incr > 0) ? ((*pupper - *plower) / incr) + 1 : ((*plower - *pupper) / (-incr)) + 1;
    int nthreads = thread->team->priv.nthreads;
    int tid      = thread->tid;

	switch (schedtype)
	{
        default:
            LOGGER_FATAL("Not implemented");

		case kmp_sch_static:
		{
			if (trip_count <= nthreads)
			{
				if (tid < trip_count)
				{
					*pupper = *plower = *plower + tid * incr;
                    if (plastiter)
                        *plastiter = (tid == trip_count - 1);
                }
				else
				{
					*plower = *pupper + incr;
					return;
				}

				return ;
			}
			else
			{
				int chunk_size = trip_count / nthreads;
				int extras = trip_count % nthreads;

				if (tid < extras)
				{
					/* The first part is homogeneous with a chunk size a little bit larger */
					*pupper = *plower + (tid + 1) * (chunk_size + 1) * incr - incr;
					*plower = *plower + tid * (chunk_size + 1) * incr;
				}
				else
				{
					*pupper = *plower + extras * (chunk_size + 1) * incr +
					          (tid + 1 - extras) * chunk_size * incr - incr;
					*plower = *plower + extras * (chunk_size + 1) * incr +
					          (tid - extras) * chunk_size * incr;
				}

				if (plastiter)
				{
					if (incr > 0)
						*plastiter = *plower <= pupper_old && *pupper > pupper_old - incr;
					else
						*plastiter = *plower >= pupper_old && *pupper < pupper_old - incr;
				}
			}

			break;
		}
	}
}
