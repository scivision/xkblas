/*
** Copyright 2009-2013,2018,2019 INRIA
**
** Contributors :
**
** thierry.gautier@inrialpes.fr
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

#include <dlfcn.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "common.h"
#include "kaapi_impl.h"
#include "kaapi_offload.h"

#if KAAPI_USE_CUDA 
#  include <cublas_v2.h>
#elif KAAPI_USE_HIP
#  include <hipblas/hipblas.h>
#endif

#if KAAPI_USE_CUDA || KAAPI_USE_HIP
//#include <cuda.h>
#  include <cuda_runtime_api.h>
#endif

#include "internal_register.h"

/* 2^KAAPI_SIZE_DSM_MAP is the size of the hash map */
#define KAAPI_SIZE_DSM_MAP 20

/* XKBLAS_ADAPTATIVE : if defined to 1 then map thread to fewer GPU and control task granularity
   To be extended to a set of GPUs.
   Dependency: require omp.h in order to detect if thread is in parallel region.
   Next implementation will only be based on internal counter such as GPU activites.
*/
static int use_partition_thread_strategy = 0;

#define XKBLAS_ADAPTATIVE 1

#if XKBLAS_ADAPTATIVE==1
#include <omp.h>
#endif

static kaapi_team_t*       _xkblas_global_team = 0;
static kaapi_atomic_t      _xkblas_thread_idx = {0};
__thread xkblas_context_t* _xkblas_self_context = 0;
pthread_key_t _pthread_xkblas_context_key;

/* deprecated variable */
__thread kaapi_thread_t*   _xkblas_self_thread = 0;

/* list of all contexts declared */
static kaapi_lock_t        _xkblas_list_lock = KAAPI_LOCK_INITIALIZER;
static xkblas_context_t*   _xkblas_list_context = 0;

static xkblas_mode_math_t xkblas_default_math = XKBLAS_DEFAULT_MATH;

/* */
static const char* get_xkblas_info(void);


/* default tile size */
static size_t NB = 0;

void xkblas_activate_custom_alloc(){}
void xkblas_deactivate_custom_alloc(){}

/*
uint64_t xkblas_register_memory_async( void* ptr, size_t sz )
{
#if KAAPI_USE_CUDA||KAAPI_USE_HIP
// warning in this version, if USE_HIP is defined, then also is USE_CUDA 
// (the file is hipyfied to be compiled with hip)
#if KAAPI_USE_HIP
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#elif KAAPI_USE_CUDA 
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
  if (driver ==0) return 0;
  return driver->f_host_register( ptr, sz, 0, 0, 0, 0);
#endif
  return 0;
}
*/
void xkblas_host_register_direct(void* ptr,size_t size)
{
#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif
	driver->f_host_register_direct( ptr, size );
	
}

void xkblas_host_unregister_direct(void* ptr)
{
#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif
	driver->f_host_unregister_direct( ptr );
}

void xkblas_memset(void* ptr, int val, size_t count)
{
#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif
	driver->f_memset( ptr, val, count );
}

void xkblas_memcpy(void* dst, void* src, size_t count)
{
#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif
	driver->f_memcpy( dst, src, count );
}

#if defined(KAAPI_UNIFIED)
//kaapi_driver_t* _last_used_driver; // Used to allow free after malloc... not clean but current MUMPS version finalize xkblas before freeing TODO FIX
void (*_last_free_to_use)(void*) = NULL;
#endif

void xkblas_malloc_unified(void** ptr, size_t size)
{
#if defined(KAAPI_UNIFIED)
#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif
	_last_free_to_use = driver->f_free_unified;
	//_last_used_driver = driver; // Assume allocation is always done when xkblas is initialized ... TODO FIX

	driver->f_malloc_unified( ptr, size );
	//printf("Allocate unified %p %p\n", *ptr, _last_used_driver);
#endif
}

void xkblas_free_unified(void* ptr)
{
#if defined(KAAPI_UNIFIED)
	//printf("Free unified %p %p\n", ptr, _last_used_driver);
#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif
	if(driver == 0)
	{
	//	driver = _last_used_driver; // xkblas has been cleaned but data are still allocated... TODO FIX
		_last_free_to_use( ptr );
	}
	else
	{
		driver->f_free_unified( ptr );
	}
#endif
}

/* Deallocate the current xkblas_context */
static void
xkblas_pthread_context_clean(void* arg)
{
    xkblas_context_t* xkblas_ctxt = _xkblas_self_context;
    kaapi_atomic_lock(&_xkblas_list_lock);
    if( xkblas_ctxt != NULL )
    {
      if( xkblas_ctxt->prev != NULL )
          xkblas_ctxt->prev->next = xkblas_ctxt->next;
      if( xkblas_ctxt->next != NULL )
          xkblas_ctxt->next->prev = xkblas_ctxt->prev;
      if( _xkblas_list_context == xkblas_ctxt )
          _xkblas_list_context = xkblas_ctxt->next;
      xkblas_ctxt->next = NULL;
      xkblas_ctxt->prev = NULL;

      kaapi_team_deattach(xkblas_ctxt->kteam, xkblas_ctxt->kthread);
      kaapi_thread_unbind(xkblas_ctxt->kthread);
      kaapi_team_dealloc(xkblas_ctxt->kteam);
      xkblas_ctxt->kteam = 0;

      int err = kaapi_hashmap_clear(&xkblas_ctxt->xkblas_ptr2handle);
      kaapi_assert(err == 0);
      err = kaapi_hashmap_destroy(&xkblas_ctxt->xkblas_ptr2handle);
      kaapi_assert(err == 0);

      *xkblas_ctxt->self = 0; // equivalent to _xkblas_self_context == NULL
      free(xkblas_ctxt);
    }
    kaapi_atomic_unlock(&_xkblas_list_lock);
}

/* Return the current xkblas_context (
*/
xkblas_context_t* xkblas_context_alloc(void)
{
  if (_xkblas_self_context ==0)
  {
    /* todo: data structure compaction: between kaapi context and xkblas context
         |------------------|
         |                  |
         | kaapi_context_t  |
         |                  |
         |------------------|
         |                  |
         | xkblas_context_t |
         |                  |
         |------------------|

      Todo that-> use user_size extra context in thread_bind.

      Use low level kaapi thread routines and do explicit register to be able
      to free/unbind data on finalize
    */
    kaapi_context_t* kctxt = _kaapi_self_context = kaapi_init_get_context();
    kaapi_thread_t* kthread = kaapi_context2thread(kctxt);
    kaapi_assert( kthread != 0);
    _xkblas_self_thread = kthread;

    xkblas_context_t* ctxt = (xkblas_context_t*)malloc(sizeof(xkblas_context_t));
    int err = kaapi_hashmap_init(&ctxt->xkblas_ptr2handle, ctxt->xkblas_mapentries, KAAPI_SIZE_DSM_MAP, 0);
    kaapi_assert(err ==0);
    ctxt->xkblas_list_sync0 = 0;
    ctxt->xkblas_list_sync0_tail = 0;
    ctxt->xkblas_generation_cache = 0;
    ctxt->xkblas_matrix_descr_list = 0;
    ctxt->xkblas_modemath = xkblas_default_math;
    ctxt->kctxt = kctxt;
    ctxt->kthread = kthread;
    ctxt->NB = NB; /* copie defaut value */
    if (use_partition_thread_strategy ==1)
    {
      ctxt->ngpus = -1;    /* no gpu defined, take all of them */
      ctxt->ngpus  = kaapi_default_param.ngpus;
      kaapi_assert( ctxt->ngpus < XKBLAS_MAX_NGPUS);
      for (int i=0; i<ctxt->ngpus; ++i)
        ctxt->gpuset[i] = i;
    }
    else
    {
      ctxt->ngpus = -1;    /* each XKBLAS is mapped to 1 GPU */
      ctxt->ngpus  = kaapi_default_param.ngpus;
      for (int i=0; i<XKBLAS_MAX_NGPUS; ++i)
        ctxt->gpuset[i] = i;
    }

    /* create default kaapi context & thread */
    int idx = KAAPI_ATOMIC_INCR(&_xkblas_thread_idx);
    ctxt->kteam = kaapi_team_alloc();
    kaapi_team_attach(ctxt->kteam, kthread, 0 ); /* idx. But with 1 team per thread: all threads have idx 0 */
    kaapi_begin_dfg( kthread, KAAPI_FRAME_FLAG_DFG_OK );

    /* link all context */
    kaapi_atomic_lock(&_xkblas_list_lock);
    ctxt->prev = NULL;
    ctxt->next = _xkblas_list_context;
    if(ctxt->next != NULL)
      ctxt->next->prev = ctxt;

    _xkblas_list_context = ctxt;
    ctxt->self = &_xkblas_self_context;
    kaapi_atomic_unlock(&_xkblas_list_lock);
    _xkblas_self_context = ctxt;

    pthread_setspecific( _pthread_xkblas_context_key, _xkblas_self_context);
  }
  return _xkblas_self_context;
}



/*
*/
uint32_t xkblas_context_get_generation()
{ return xkblas_context_get()->xkblas_generation_cache; }

/*
*/
void xkblas_set_modemath( xkblas_mode_math_t m)
{
  xkblas_context_get()->xkblas_modemath = m;
}

/*
*/
xkblas_mode_math_t xkblas_get_modemath(void)
{
  return xkblas_context_get()->xkblas_modemath;
}

/*
*/
extern kaapi_handle_t* xkblas_context_get_list_sync0(void)
{
  return xkblas_context_get()->xkblas_list_sync0;
}

/*
*/
static void* handle_cpublas = 0;

/*
*/
kaapi_handle_t* xkblas_get_handle(
  xkblas_matrix_descr_t* Ah,
  size_t m,
  size_t n)
{ 
  kaapi_assert_debug( (m<Ah->mt) && (n<Ah->nt) );
  return &Ah->handle[m*Ah->nt+n];
}

/*
*/
uint16_t xkblas_get_ldid(
  xkblas_matrix_descr_t* Ah,
  size_t m,
  size_t n)
{
  kaapi_assert_debug( (m<Ah->mt) && (n<Ah->nt) );
  return Ah->ldid[m*Ah->nt+n];
}

/*
*/
uint16_t xkblas_set_ldid(
  xkblas_matrix_descr_t* Ah,
  size_t m,
  size_t n,
  uint16_t val)
{
  kaapi_assert_debug( (m<Ah->mt) && (n<Ah->nt) );
  return Ah->ldid[m*Ah->nt+n] = val;
}


/* Init if also same generation
*/
int xkblas_matrix_descr_isinit(
  xkblas_matrix_descr_t* Ah
)
{
  return (Ah->addr != 0);
}


/* Find the internal descriptor for A
*/
xkblas_matrix_descr_t* xkblas_find( const void* A )
{
  xkblas_matrix_descr_t** me;
  kaapi_hashentries_t* entry;
  xkblas_context_t* ctxt = xkblas_context_get();

  entry = kaapi_hashmap_findinsert( &ctxt->xkblas_ptr2handle, A );
  me = KAAPI_HASHENTRIES_GETREF(entry, xkblas_matrix_descr_t*);
  if (*me == 0)
  {
    xkblas_matrix_descr_t* Ah = (xkblas_matrix_descr_t*)malloc(sizeof(struct xkblas_matrix_descr));
    /* marker for xkblas_matrix_descr_isinit return false */
    Ah->handle = 0;
    Ah->addr = 0;
    Ah->capacity = 0;
    Ah->next = ctxt->xkblas_matrix_descr_list;
    Ah->entry = (void*)entry;
    ctxt->xkblas_matrix_descr_list = Ah;
#if KAAPI_DEBUG
    Ah->owner = pthread_self();
#endif
    *me = Ah;
  }
#if KAAPI_DEBUG
  else {
    kaapi_assert((*me)->owner == pthread_self());
  }
#endif
  return *me;
}


int xkblas_init_matrix_handle( xkblas_matrix_descr_t* Ah,
  void* A, size_t M, size_t N, size_t LD, size_t eltsize, size_t MB, size_t NB
)
{
  xkblas_context_t* ctxt = xkblas_context_get();

  Ah->addr = A;
  Ah->eltsize = eltsize;
  Ah->ld = LD;
  Ah->M  = M;
  Ah->N  = N;
  Ah->mb = MB;
  Ah->nb = NB;
#if KAAPI_DEBUG
  Ah->owner = pthread_self();
#endif

  size_t mt = xkblas_num_of_tiles(M,MB);
  size_t nt = xkblas_num_of_tiles(N,NB);
  if (mt * nt >= Ah->capacity)
  {
    if (Ah->handle) free(Ah->handle);
    Ah->handle = malloc( (sizeof(uint16_t) + sizeof(kaapi_handle_t))* mt * nt );
    Ah->capacity = mt*nt;
  }
  Ah->ldid   = (uint16_t*)(Ah->handle + mt * nt);
  Ah->mt = mt;
  Ah->nt = nt;

  for (size_t m=0; m< Ah->mt; ++m)
    for (size_t n=0; n< Ah->nt; ++n)
    {
#define ADDR_(A, m, n) ((char*)(A)+ ((m) * MB + (n) * NB * LD)*eltsize)
      kaapi_handle_t*  handle = &Ah->handle[m*Ah->nt+n];
      kaapi_handle_init(ctxt->kthread, handle, ADDR_(A,m,n), 0);
#if BUG_2022_03_18
printf("%p:: %30.30s matrix: %p %lix%li (dim), %lix%li (tile), tile: (%lix%li), new handle: %p\n",pthread_self(), __FUNCTION__, A, M, N, mt, nt, MB, NB, handle);
      handle->sync0.sync = 0;
      if (ctxt->xkblas_list_sync0_tail !=0)
        ctxt->xkblas_list_sync0_tail->sync0.sync = (kaapi_access_t*)handle;
      else
        ctxt->xkblas_list_sync0 = handle;
      ctxt->xkblas_list_sync0_tail = handle;
#else
      handle->sync0.sync = (kaapi_access_t*)ctxt->xkblas_list_sync0;
      ctxt->xkblas_list_sync0 = handle;
#endif
      Ah->ldid[m*Ah->nt+n] = (uint16_t)-1;
    }

  /* to debug... */
  Ah->gen = ctxt->xkblas_generation_cache;
  return 0;
}


/* Find the mapping for the tile A(i,j)
*/
uint16_t xkblas_get_ld(
  xkblas_matrix_descr_t* Ah,
  size_t i, size_t j
)
{
  uint16_t lid0 = kaapi_memory_asid_get_lid( kaapi_local_asid );
  uint16_t lid;

  if (Ah ==0) return (uint16_t)-1;

  kaapi_assert_debug( (i<Ah->mt) && (j<Ah->nt) );
  lid = Ah->ldid[ i*Ah->nt + j ];

  if (lid != (uint16_t)-1)
    return lid;

  unsigned int count = kaapi_localitydomain_count(KAAPI_LD_GPU);
  if (count ==0)
  {
    lid = lid0;
    goto retval;
  }

  kaapi_handle_t* h = xkblas_get_handle(Ah, i, j);
  kaapi_metadata_info_t* mdi = h->last->mdi;
  if (mdi !=0)
  {
    int idxvalid[KAAPI_MEMORY_MAX_NODES];
    int count = 0;
    KAAPI_MEMORY_VALUE_TYPE valid_bit= KAAPI_ATOMIC_READ(&mdi->valid);
    valid_bit &= ~(1<<lid0);
    while (valid_bit !=0)
    {
      KAAPI_MEMORY_VALUE_TYPE lid = KAAPI_MEMORY_FFS(valid_bit);
      --lid;
      if (kaapi_memory_replica_is_valid(mdi, lid))
        idxvalid[count++] = lid;
      valid_bit &= ~(1<<lid);
    }
//TG: to do here avoid to talk of ldid : locality domain id and lid: local id of the adress space. Should be ~ the same ?
    kaapi_assert(0);
    if (count ==1)
      return 1+idxvalid[0];  
    else if (count >0)
      return 1+idxvalid[rand() % count];  
  }
  lid = (uint16_t)-1;

retval:
  Ah->ldid[ i*Ah->nt + j ] = lid;
  return lid;
}


/* NB = tile size. 0 == value computed at runtime
*/
void xkblas_set_param(size_t nb, size_t p)
{
  //printf( "%s::Set tile size to: %lu\n", __func__, nb );
  NB = nb;
  if (p > sizeof(double)) /* max precision */
    p = 16;
}


/*
*/
size_t xkblas_get_param(void)
{
  xkblas_context_t* xkctxt = xkblas_context_get();
  return xkctxt->NB;
}


int xkblas_get_devicecount(void)
{
  return 0;
}


/*
*/
int xkblas_get_ngpus(void)
{
  xkblas_context_t* xkctxt = xkblas_context_get();
  return (int)xkctxt->ngpus;
}

/*
*/
int xkblas_set_ngpus(int ngpus)
{
  int err = 0;
  if (ngpus <0) return EINVAL;
  if (ngpus > kaapi_default_param.sys_ngpus)
  {
    err = EDOM;
    ngpus = kaapi_default_param.sys_ngpus;
  }
  xkblas_context_t* xkctxt = xkblas_context_get();
  xkctxt->ngpus = ngpus;
  return err;
}

/*
 */
void* xkblas_malloc( size_t size )
{
#if KAAPI_USE_HIP
  void* ptr = 0;
  kaapi_assert_m(hipSuccess== hipHostMalloc(&ptr, size, hipHostMallocPortable),"hipHostAlloc failed");
  return ptr;
#elif KAAPI_USE_CUDA  
  void* ptr = 0;
  kaapi_assert_m(cudaSuccess== cudaHostAlloc(&ptr, size, cudaHostAllocPortable),"cudaHostAlloc failed");
  return ptr;
#else
  return malloc(size);
#endif
}

/*
 */
void xkblas_free( void* ptr, size_t sz )
{
#if KAAPI_USE_CUDA || KAAPI_USE_HIP
  //kaapi_assert_m(CUDA_SUCCESS==  cuMemFreeHost(ptr),"cuMemFreeHost failed");
  kaapi_assert_m(cudaSuccess== cudaFreeHost(ptr),"cudaFreeHost failed");
#else
  free(ptr);
#endif
}


/* Asynchronous operation, return handle to the request
 */
uint64_t xkblas_register_memory_async( void* ptr, size_t sz )
{
#if KAAPI_USE_CUDA||KAAPI_USE_HIP
// warning in this version, if USE_HIP is defined, then also is USE_CUDA 
// (the file is hipyfied to be compiled with hip)
#if KAAPI_USE_HIP
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#elif KAAPI_USE_CUDA 
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
  if (driver ==0) return 0;
  return driver->f_host_register( ptr, sz, 0, 0, 0, 0);
#endif
  return 0;
}


/*
*/
uint64_t xkblas_unregister_memory_async( void* ptr, size_t sz )
{
#if KAAPI_USE_CUDA || KAAPI_USE_HIP
// warning in this version, if USE_HIP is defined, then also is USE_CUDA 
// (the file is hipyfied to be compiled with hip)
#if KAAPI_USE_HIP
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#elif KAAPI_USE_CUDA 
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
  if (driver ==0) return 0;
  return driver->f_host_unregister( ptr, sz, 0, 0, 0, 0);
#endif
  return 0;
}


/* Test if the request is completed
*/
int xkblas_register_memory_test( uint64_t handle )
{
#if KAAPI_USE_CUDA || KAAPI_USE_HIP
// warning in this version, if USE_HIP is defined, then also is USE_CUDA 
// (the file is hipyfied to be compiled with hip)
#if KAAPI_USE_HIP
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#elif KAAPI_USE_CUDA 
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
  if (driver ==0) return 1; /* always completed */
  return driver->f_host_register_testwait( handle, 0 );
#endif
  return 0;
}


/* Return the error code of the memory registration request handle
*/
int xkblas_register_memory_wait( uint64_t handle )
{
#if KAAPI_USE_CUDA || KAAPI_USE_HIP
// warning in this version, if USE_HIP is defined, then also is USE_CUDA 
// (the file is hipyfied to be compiled with hip)
#if KAAPI_USE_HIP
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#elif KAAPI_USE_CUDA 
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
  if (driver ==0) return 1; /* always completed */
  return driver->f_host_register_testwait( handle, 1 );
#endif
  return 0;
}


/*
*/
int xkblas_register_memory_waitall( )
{
#if KAAPI_USE_CUDA || KAAPI_USE_HIP
// warning in this version, if USE_HIP is defined, then also is USE_CUDA 
// (the file is hipyfied to be compiled with hip)
#if KAAPI_USE_HIP
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#elif KAAPI_USE_CUDA 
  kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
  if (driver ==0) return 1; /* always completed */
  return driver->f_host_register_testwait( (uint64_t)-1, 2 );
#endif
  return 0;
}


/*
 */
int xkblas_register_memory( void* ptr, size_t sz )
{
  return xkblas_register_memory_wait( xkblas_register_memory_async(ptr, sz) );
}

/*
 */
int xkblas_unregister_memory( void* ptr, size_t sz )
{
  uint64_t handle = xkblas_unregister_memory_async(ptr, sz); 
  return xkblas_register_memory_waitall();
}

/*
*/
#if 00
static void xkblas_free_curr_blochandle(void)
{
  while (curr_blochandle !=0)
  {
    kaapi_stack_bloc_t* bloc = curr_blochandle;
    curr_blochandle = bloc->next;
    bloc->next = freelist_blochandle;
    freelist_blochandle = bloc;
  }
}
#endif

/* Map all block to 1 GPU
*/
int xkblas_map_all(
  xkblas_context_t* xkctxt,
  int hlevel, int storage, size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize,
  int gpu, int force
)
{
  xkblas_matrix_descr_t* Ah = xkblas_find(A);
  if (!xkblas_matrix_descr_isinit(Ah)) return EINVAL;

  kaapi_ld_type_t type;
  switch (hlevel) {
    case 0: type = KAAPI_LD_BOARD; break;
    case 1: type = KAAPI_LD_GPU; break;
    case 2: type = KAAPI_LD_CORE; break;
    default:
      printf("[%s] unknown type, returns immediatly\n", __func__);
#if KAAPI_DEBUG
      abort();
#endif
      return EINVAL;
  };

  unsigned int count = kaapi_localitydomain_count(type);
  if (count ==0) return EINVAL;

  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;

  kaapi_localitydomain_t* ld = kaapi_localitydomain_get_bytype(type, gpu);

  for (size_t i=0; i<Amt; ++i)
  {
    for (size_t j=0; j<Ant; ++j)
    {
      uint16_t ldid = xkblas_get_ldid(Ah, i, j );
      if ((ldid ==(uint16_t)-1) || force)
      {
        xkblas_set_ldid(Ah, i, j, ldid = ld->ldid);
#if KAAPI_USE_OCR
        kaapi_assert( ldid == kaapi_memory_asid_get_lid(ld->device->memdev.asid) );
        kaapi_assert(0 == kaapi_dsm_wish_distribution(
              &kaapi_the_dsm,
              ld->device->memdev.asid,
              xkblas_get_handle(Ah, i, j)
        ));
#endif
      }
    }
  }
  return 0;
}

/* Do not implement type== ALL
   store bloc (i,j) on a grid of ressource GpxGq (i/Bp)%Gp,(j/Bq)%Gq
   If matrix is not found return EINVAL
*/
int xkblas_map_2Dblock_cyclic(
  xkblas_context_t* xkctxt,
  int hlevel, int storage, size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize,
  size_t Bp, size_t Bq, /* blocking size */
  size_t Gp, size_t Gq,  /* grid size */
  int force
)
{
  xkblas_matrix_descr_t* Ah = xkblas_find(A);
  if (!xkblas_matrix_descr_isinit(Ah)) return EINVAL;

  kaapi_ld_type_t type;
  switch (hlevel) {
    case 0: type = KAAPI_LD_BOARD; break;
    case 1: type = KAAPI_LD_GPU; break;
    case 2: type = KAAPI_LD_CORE; break;
    default:
      printf("[%s] unknown type, returns immediatly\n", __func__);
#if KAAPI_DEBUG
      abort();
#endif
      return EINVAL;
  };

  unsigned int count = kaapi_localitydomain_count(type);
  if ((use_partition_thread_strategy) && (type ==KAAPI_LD_GPU))
    count = xkctxt->ngpus;

  if (count ==0) return EINVAL;

  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;

  for (size_t i=0; i<Amt; ++i)
  {
    for (size_t j=0; j<Ant; ++j)
    {
      uint16_t ldid = xkblas_get_ldid(Ah, i, j );
      if ((ldid ==(uint16_t)-1) || force)
      {
        int r = ( ((i/Bp)%Gp)*Gq + (j/Bq)%Gq ) %count;

        if ((use_partition_thread_strategy) && (type ==KAAPI_LD_GPU))
        {
          int* gpuset = xkctxt->gpuset;
          kaapi_assert( r<xkctxt->ngpus );
          r = gpuset[r];
        }

        kaapi_localitydomain_t* ld = kaapi_localitydomain_get_bytype(type, r);
        xkblas_set_ldid(Ah, i, j, ldid = ld->ldid);
#if KAAPI_USE_OCR
        kaapi_assert( ldid == kaapi_memory_asid_get_lid(ld->device->memdev.asid) );
        kaapi_assert(0 == kaapi_dsm_wish_distribution(
              &kaapi_the_dsm,
              ld->device->memdev.asid,
              xkblas_get_handle(Ah, i, j)
        ));
#endif
      }
    }
  }
  return 0;
}

/* Do not implement type== ALL
   store bloc [i,.] on a ressource (i/B)%G
   colrow = 0 -> col mapping
   colrow = 1 -> row mapping
*/
int xkblas_map_1Dblock_cyclic(
  xkblas_context_t* xkctxt,
  int hlevel, int storage, int colrow, size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize,
  size_t B, size_t G,    /* grid size */
  int force
)
{
  size_t MB,NB;
  MB = NB = xkblas_get_param();
  if (colrow == 0)
  {
    return xkblas_map_2Dblock_cyclic(xkctxt,
      hlevel, storage, m, n, A, lda, eltsize,
      (m+MB-1)/MB, B,
      1, G,
      force
    );
  }
  else
  {
    return xkblas_map_2Dblock_cyclic(xkctxt,
      hlevel, storage, m, n, A, lda, eltsize,
      B, (n+NB-1)/NB,
      G, 1,
      force
    );
  }
#if KAAPI_DEBUG
  abort();
#endif
  return EINVAL;
}


int xkblas_map_cyclic(
  xkblas_context_t* xkctxt,
  int hlevel, int storage, size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize,
  int force
)
{
  xkblas_matrix_descr_t* Ah = xkblas_find(A);
  if (!xkblas_matrix_descr_isinit(Ah)) return EINVAL;

  kaapi_ld_type_t type;
  switch (hlevel) {
    case 0: type = KAAPI_LD_BOARD; break;
    case 1: type = KAAPI_LD_GPU; break;
    case 2: type = KAAPI_LD_CORE; break;
    default:
      printf("[%s] unknown type, returns immediatly\n", __func__);
#if KAAPI_DEBUG
      abort();
#endif
      return EINVAL;
  };

  unsigned int count = kaapi_localitydomain_count(type);
  if ((use_partition_thread_strategy) && (type ==KAAPI_LD_GPU))
    count = xkctxt->ngpus;
  if (count ==0) return EINVAL;

  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;
  char* ptr = (char*)A;
  void* addr;

  for (size_t i=0; i<Amt; ++i)
  {
    for (size_t j=0; j<Ant; ++j)
    {
      uint16_t ldid = xkblas_get_ldid(Ah, i, j );
      if ((ldid ==(uint16_t)-1) || force)
      {
        int r = (i*Ant+j)%count;
        if ((use_partition_thread_strategy) && (type ==KAAPI_LD_GPU))
        {
          int* gpuset = xkctxt->gpuset;
          kaapi_assert( r<xkctxt->ngpus );
          r = gpuset[r];
        }
        xkblas_set_ldid(Ah, i, j, ldid = kaapi_localitydomain_get_num(type, r));
      }
    }
  }
  return 0;
}


int xkblas_map_ij_cyclic(
  xkblas_context_t* xkctxt,
  int hlevel, int storage, size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize,
  int force
)
{
  xkblas_matrix_descr_t* Ah = xkblas_find(A);
  if (!xkblas_matrix_descr_isinit(Ah)) return EINVAL;

  kaapi_ld_type_t type;
  switch (hlevel) {
    case 0: type = KAAPI_LD_BOARD; break;
    case 1: type = KAAPI_LD_GPU; break;
    case 2: type = KAAPI_LD_CORE; break;
    default:
      printf("[%s] unknown type, returns immediatly\n", __func__);
#if KAAPI_DEBUG
      abort();
#endif
      return EINVAL;
  };

  unsigned int count = kaapi_localitydomain_count(type);
  if ((use_partition_thread_strategy) && (type ==KAAPI_LD_GPU))
    count = xkctxt->ngpus;
  if (count ==0) return EINVAL;

  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;
  char* ptr = (char*)A;
  void* addr;

  for (size_t i=0; i<Amt; ++i)
  {
    for (size_t j=0; j<Ant; ++j)
    {
      uint16_t ldid = xkblas_get_ldid(Ah, i, j );
      if ((ldid ==(uint16_t)-1) || force)
      {
        int r = (i+j)%count;
        if ((use_partition_thread_strategy) && (type ==KAAPI_LD_GPU))
        {
          int* gpuset = xkctxt->gpuset;
          kaapi_assert( r<xkctxt->ngpus );
          r = gpuset[r];
        }
        xkblas_set_ldid(Ah, i, j, ldid = kaapi_localitydomain_get_num(type, r));
      }
    }
  }
  return 0;
}

#if 0
int xkblas_map_test(
  xkblas_context_t* xkctxt,
  int hlevel, int storage, size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize,
  int force
)
{
  xkblas_matrix_descr_t* Ah = xkblas_find(A);
  if (!xkblas_matrix_descr_isinit(Ah)) return EINVAL;

  kaapi_ld_type_t type;
  switch (hlevel) {
    case 0: type = KAAPI_LD_BOARD; break;
    case 1: type = KAAPI_LD_GPU; break;
    case 2: type = KAAPI_LD_CORE; break;
    default:
      printf("[%s] unknown type, returns immediatly\n", __func__);
#if KAAPI_DEBUG
      abort();
#endif
      return EINVAL;
  };

  unsigned int count = kaapi_localitydomain_count(type);
  if (count ==0) return EINVAL;

  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;
  char* ptr = (char*)A;
  void* addr;
  size_t B = (Ant+2*count-1)/(2*count);

  for (size_t i=0; i<Amt; ++i)
  {
    int r = i % count;
    uint16_t ldid = xkblas_get_ldid(Ah, i, i );
    if ((ldid ==(uint16_t)-1) || force)
      xkblas_set_ldid(Ah, i, i, ldid = kaapi_localitydomain_get_num(type, r));

    size_t c = i+1;
    for (size_t j=0; j<Ant; ++j)
    {
      if (i ==j) continue;
      int r = c % count;
      ++c;
      uint16_t ldid = xkblas_get_ldid(Ah, i, j );
      if ((ldid ==(uint16_t)-1) || force)
        xkblas_set_ldid(Ah, i, j, ldid = kaapi_localitydomain_get_num(type, r));
    }
  }
  return 0;
}

#endif

/*
*/
#define STRNAME  "writeback"
#define NAME(x) kaapi_##x##_writeback
#define PNAME(x) kaapi_writeback_##x
#define SIZE_NPARAM 1
#define NPARAM 1
#define MODE_PARAM {KAAPI_ACCESS_MODE_R}
#define ADDR_PARAM {&arg->a}
#define VIEW_PARAM {{ &arg->m, &arg->n, &arg->ld}}
#define FORMAT_TYPE kaapi_char_format
#define SIZEOF_TYPE arg->eltsize
#define DOT_COLOR "gray"
#define DOT_SHAPE "octagon"
#define TASK_FLOPS 0
#define TASK_DATA  0


typedef struct {
  kaapi_access_t a;
  size_t m;
  size_t n;
  size_t ld;
  size_t eltsize;
  kaapi_frame_t* frame;
} NAME(Arg);
static kaapi_format_id_t NAME(task_fmtid) = 0;

/*
*/
static void NAME(task_body_cpu)( kaapi_task_t* task, kaapi_thread_t* thread )
{
}

/*
*/
#if KAAPI_DEBUG && (KAAPI_USE_CUDA || KAAPI_USE_HIP)
#  define KAAPI_DEBUG_CTR 1
#endif

#if (KAAPI_USE_CUDA || KAAPI_USE_HIP)
#  if KAAPI_DEBUG_CTR
#endif
#if (KAAPI_USE_CUDA || KAAPI_USE_HIP)
kaapi_atomic_t spawn_writeback={0};
kaapi_atomic_t pending_writeback={0};
kaapi_atomic_t received_writeback={0};
#  endif
static void callback_epilogue_writeback(
    kaapi_io_status_t status,
    kaapi_io_stream_t* ios,
    void* arg0, void* arg1, void* arg2
)
{
  kaapi_metadata_info_t* mdi __attribute__((unused)) = (kaapi_metadata_info_t*)arg0;
  kaapi_frame_t* frame = (kaapi_frame_t*)arg1;
#  if KAAPI_DEBUG_CTR
  KAAPI_ATOMIC_INCR(&received_writeback);
#  endif
  KAAPI_ATOMIC_INCR(&frame->exec_count);
}

static void NAME(task_body_gpu)( kaapi_task_t* task, kaapi_thread_t* thread, void* handle )
{
  /* Make execution as if task_body_gpu spawn continuation (reception of communication)
     in order to detect that end of tasks execution
  */
  NAME(Arg)* taskarg = kaapi_task_getargst(task,NAME(Arg));
#  if KAAPI_DEBUG_CTR
  KAAPI_ATOMIC_INCR(&pending_writeback);
#  endif
  int err = kaapi_dsm_prefetch_on( &kaapi_the_dsm, kaapi_local_asid,
    taskarg->a.mdi, 
    callback_epilogue_writeback, taskarg->a.mdi, taskarg->frame, taskarg
  );
  kaapi_assert((err==0) || (err== EINPROGRESS));
  if (err ==0) /* no call back called in that case, unroll callback_epilogue_writeback */
    KAAPI_ATOMIC_INCR(&taskarg->frame->exec_count);
}
#endif

#include "task_format.h"


/* Create write back task for the tile A(m,n) of size compute in the function
  TODO
   - there is may be two problems here:
     * the task makes its action (recopy data from GPU) in a split-phase actions
     - 1/ the task post request to kaapi to fetch data on the local host CPU
     - 2/ upon completion the callback is called to signal the end of the transfert
  1/ followed by 2/ is not atomic. It is possible to schedule a task that update the
  data between 1/ and 2/. So what would be the transfered data ?
  If the data is already local, kaapi_dsm_prefetch_on never calls the callback function.
  It should be.
*/
static void xkblas_create_taskwriteback(
  xkblas_matrix_descr_t* Ah,
  size_t m, size_t n,
  size_t ld,
  size_t eltsize
)
{
  kaapi_task_t* task;
  kaapi_thread_t* thread = xkblas_self_thread();
  kaapi_context_t* ctxt = kaapi_thread2context(thread);
  size_t tasksize = sizeof(kaapi_Arg_writeback) + sizeof(kaapi_task_t);
  task = kaapi_task_alloc( thread, kaapi_task_fmtid_writeback, tasksize );
  kaapi_Arg_writeback* taskarg = kaapi_task_getargst(task,kaapi_Arg_writeback); // == NAME(ARG)

  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;
  taskarg->m = m == Amt-1 ? Ah->M - m*Ah->mb : Ah->mb;
  taskarg->n = n == Ant-1 ? Ah->N - n*Ah->nb : Ah->nb;
  taskarg->eltsize = eltsize;
  taskarg->ld = ld;
  taskarg->frame = ctxt->unlink;
  kaapi_update_dependencies(thread, &taskarg->a, task,
      KAAPI_ACCESS_MODE_R, xkblas_get_handle(Ah,m,n));
  kaapi_assert_debug( taskarg->a.mdi==0 );
  kaapi_taskflag_set(task, KAAPI_TASK_FLAG_INCOM);

#if KAAPI_USE_OCR
  /* OCR on the first parameter */
  kaapi_task_set_ld(task, KAAPI_TASK_OCR_PARAM, 0);
#else
  uint16_t ldid = xkblas_get_ld(Ah,m,n);
  kaapi_task_set_ld(task, KAAPI_TASK_LD_BOUND, ldid);
#endif


#if KAAPI_DEBUG_CTR
  KAAPI_ATOMIC_INCR(&spawn_writeback);
#endif
  /* incr spawn_count: spawned task should be considered completed when epilogue is called */
  KAAPI_ATOMIC_INCR(&taskarg->frame->spawn_count);
  kaapi_task_commit( thread, task );
}


/*
*/
#define STRNAME  "invalidate"
#define NAME(x) kaapi_##x##_invalidate
#define PNAME(x) kaapi_invalidate_##x
#define SIZE_NPARAM 1
#define NPARAM 1
#define MODE_PARAM {KAAPI_ACCESS_MODE_RW}
#define ADDR_PARAM {&arg->a}
#define VIEW_PARAM {{ &arg->m, &arg->n, &arg->ld}}
#define FORMAT_TYPE kaapi_char_format
#define SIZEOF_TYPE arg->eltsize
#define DOT_COLOR "gray0"
#define DOT_SHAPE "invtriangle"
#define TASK_FLOPS 0
#define TASK_DATA  0

typedef struct {
  kaapi_access_t a;
  size_t m;
  size_t n;
  size_t ld;
  size_t eltsize;
  kaapi_frame_t* frame;
} NAME(Arg);
static kaapi_format_id_t NAME(task_fmtid) = 0;

/*
*/
static void NAME(task_body_cpu)( kaapi_task_t* task, kaapi_thread_t* thread )
{
  printf("[%s] should never be call on CPU !!!\n", __func__);
  abort();
}

/*
*/
#if KAAPI_USE_CUDA || KAAPI_USE_HIP
static void NAME(task_body_gpu)( kaapi_task_t* task, kaapi_thread_t* thread, void* handle )
{
  kaapi_context_t* ctxt = kaapi_thread2context(thread);
  kaapi_assert_debug( ctxt->device !=0);
  uint16_t lidhost = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lidhost < KAAPI_MEMORY_MAX_NODES);
  uint16_t lid = kaapi_memory_asid_get_lid(ctxt->device->memdev.asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  if (lid == lidhost) return;
  NAME(Arg)* taskarg = kaapi_task_getargst(task,NAME(Arg));
  kaapi_memory_cache_invalidate_bloc(&ctxt->device->memdev, 0, taskarg->a.mdi);
}
#endif

#include "task_format.h"

/*
*/
static void xkblas_create_taskinvalidate(
  xkblas_matrix_descr_t* Ah,
  size_t m, size_t n,
  size_t ld,
  size_t eltsize
)
{
  kaapi_task_t* task;
  kaapi_thread_t* thread = xkblas_self_thread();
  kaapi_context_t* ctxt = kaapi_thread2context(thread);
  size_t tasksize = sizeof(kaapi_Arg_invalidate) + sizeof(kaapi_task_t);
  task = kaapi_task_alloc( thread, kaapi_task_fmtid_invalidate, tasksize );
  kaapi_Arg_invalidate* taskarg = kaapi_task_getargst(task,kaapi_Arg_invalidate); // == NAME(ARG)
  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;
  taskarg->m = m == Amt-1 ? Ah->M - m*Ah->mb : Ah->mb;
  taskarg->n = n == Ant-1 ? Ah->N - n*Ah->nb : Ah->nb;
  taskarg->eltsize = eltsize;
  taskarg->ld = ld;
  taskarg->frame = ctxt->unlink;
  kaapi_update_dependencies(thread, &taskarg->a, task,
      KAAPI_ACCESS_MODE_RW, xkblas_get_handle(Ah,m,n));
  uint16_t lid = xkblas_get_ld(Ah, m, n);
  kaapi_taskflag_set(task, KAAPI_TASK_FLAG_INCOM);
  if (lid != (uint16_t)-1)
    kaapi_task_set_ld(task, KAAPI_TASK_LD_BOUND, lid);
  kaapi_task_commit( thread, task );
}




/*
*/
#define STRNAME  "distribute"
#define NAME(x) kaapi_##x##_distribute
#define PNAME(x) kaapi_distribute_##x
#define SIZE_NPARAM 1
#define NPARAM 1
#define MODE_PARAM {KAAPI_ACCESS_MODE_RW}
#define ADDR_PARAM {&arg->a}
#define VIEW_PARAM {{ &arg->m, &arg->n, &arg->ld}}
#define FORMAT_TYPE kaapi_char_format
#define SIZEOF_TYPE arg->eltsize
#define DOT_COLOR "gray1"
#define DOT_SHAPE "triangle"
#define TASK_FLOPS 0
#define TASK_DATA  0

typedef struct {
  kaapi_access_t a;
  size_t m;
  size_t n;
  size_t ld;
  size_t eltsize;
  kaapi_frame_t* frame;
} NAME(Arg);
static kaapi_format_id_t NAME(task_fmtid) = 0;

/*
*/
static void NAME(task_body_cpu)( kaapi_task_t* task, kaapi_thread_t* thread )
{
}

/*
*/
#if KAAPI_USE_CUDA || KAAPI_USE_HIP
static void NAME(task_body_gpu)( kaapi_task_t* task, kaapi_thread_t* thread, void* handle )
{
}
#endif

#include "task_format.h"

/* Task to force copy of data on a given ldid
*/
static void xkblas_create_distribute(
  xkblas_matrix_descr_t* Ah,
  size_t m, size_t n,
  size_t ld,
  size_t eltsize,
  kaapi_ldid_t ldid
)
{
  kaapi_task_t* task;
  kaapi_thread_t* thread = xkblas_self_thread();
  kaapi_context_t* ctxt = kaapi_thread2context(thread);
  size_t tasksize = sizeof(kaapi_Arg_distribute) + sizeof(kaapi_task_t);
  task = kaapi_task_alloc( thread, kaapi_task_fmtid_distribute, tasksize );
  kaapi_Arg_distribute* taskarg = kaapi_task_getargst(task,kaapi_Arg_distribute); // == NAME(ARG)
  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;
  taskarg->m = m == Amt-1 ? Ah->M - m*Ah->mb : Ah->mb;
  taskarg->n = n == Ant-1 ? Ah->N - n*Ah->nb : Ah->nb;
  taskarg->eltsize = eltsize;
  taskarg->ld = ld;
  taskarg->frame = ctxt->unlink;
  kaapi_update_dependencies(thread, &taskarg->a, task,
      KAAPI_ACCESS_MODE_R, xkblas_get_handle(Ah,m,n));
  kaapi_taskflag_set(task, KAAPI_TASK_FLAG_OUTCOM);
  kaapi_task_set_ld(task, KAAPI_TASK_LD_BOUND, ldid);
  kaapi_task_commit( thread, task );
}



/*
*/
static int xkblas_ismode_math_tc(void)
{
  return xkblas_default_math == XKBLAS_TENSOR_OP_MATH;
}


/*
*/
int xkblas_get_device_count(int* count)
{ 
  if (count ==0) return EINVAL;
  *count = kaapi_offload_ndevices(); 
  return 0;
}


/*
*/
static int init_count = 0;
int xkblas_init(void)
{
  if (init_count++ !=0) return 0;

  size_t tile_size = 0;
  size_t precision = sizeof(double);

  if( pthread_key_create(&_pthread_xkblas_context_key, xkblas_pthread_context_clean) != 0 ) {
    // TODO error
    printf("XKBLAS error, unable to create pthread_key\n");
  }

  if (getenv("XKBLAS_VERBOSE"))
    setenv("KAAPI_VERBOSE",getenv("XKBLAS_VERBOSE"),1);


  if (getenv("XKBLAS_TILE_SIZE") || getenv("XKBLAS_BLOC_SIZE") || getenv("XKBLAS_PRECISION"))
  {
    if (getenv("XKBLAS_TILE_SIZE") ==0)
      tile_size = NB;
    else
      tile_size = atoi(getenv("XKBLAS_TILE_SIZE"));

    if (getenv("XKBLAS_PRECISION") ==0)
      precision = sizeof(double);
    else {
      char* p = getenv("XKBLAS_PRECISION");
      if ((strcmp(p,"float") ==0) || (strcmp(p, "real*4") ==0))
        precision = sizeof(float);
      else if ((strcmp(p,"double") ==0) || (strcmp(p, "real*8") ==0))
        precision = sizeof(double);
      else if ((strcmp(p,"complex32") ==0) || (strcmp(p, "complex*4") ==0))
        precision = sizeof(Complex32_t);
      else if ((strcmp(p,"complex64") ==0) || (strcmp(p, "complex*8") ==0))
        precision = sizeof(Complex64_t);
      else {
        fprintf(stderr,"[XKBlas] XKBLAS_PRECISION must be = float|real*4|double|read*8|complex|complex32|complex*4|complex64|complex*8.\n");
        exit(1); 
      }
    }
    //printf( "%s::Set tile size to: %lu\n", __func__, tile_size );
    xkblas_set_param( tile_size, precision );

    if (getenv("XKBLAS_VERBOSE"))
    {
      printf("xe size: %lu, precision: %lu\n",tile_size, precision);
    }
  }

  if (getenv("XKBLAS_NGPUS"))
    setenv("KAAPI_NUM_GPUS",getenv("XKBLAS_NGPUS"),1);
  else if (getenv("XKBLAS_NGPU"))
    setenv("KAAPI_NUM_GPUS",getenv("XKBLAS_NGPU"),1);

  if (getenv("XKBLAS_GPUSET"))
    setenv("KAAPI_GPUSET",getenv("XKBLAS_GPUSET"),1);

  if (getenv("XKBLAS_NCUDA_STREAMS"))
  {
    if (!getenv("XKBLAS_NSTREAMS"))
      printf("[XKBlas] deprecated 'XKBLAS_NCUDA_STREAMS' use 'XKBLAS_NSTREAMS'\n");
    setenv("KAAPI_CUDA_KERNEL_STREAM_NUMS",getenv("XKBLAS_NCUDA_STREAMS"),1);
  } 
  if (getenv("XKBLAS_NSTREAMS"))
  {
    if (getenv("XKBLAS_NCUDA_STREAMS"))
      printf("[XKBlas] deprecated 'XKBLAS_NCUDA_STREAMS' also defined, use 'XKBLAS_NSTREAMS'\n");
    setenv("KAAPI_CUDA_KERNEL_STREAM_NUMS",getenv("XKBLAS_NSTREAMS"),1);
  }

  if (getenv("XKBLAS_NKERNELS_PER_STREAM"))
  {
    if (!getenv("XKBLAS_NKERNELS"))
      printf("[XKBlas] deprecated 'XKBLAS_NKERNELS_PER_STREAM' use 'XKBLAS_NKERNELS'\n");
    setenv("KAAPI_CUDA_KERNEL_PER_STREAM",getenv("XKBLAS_NKERNELS_PER_STREAM"),1);
  }
  if (getenv("XKBLAS_NKERNELS"))
  {
    if (getenv("XKBLAS_NKERNELS_PER_STREAM"))
      printf("[XKBlas] deprecated 'XKBLAS_NKERNELS_PER_STREAM' also defined, use 'XKBLAS_NKERNELS'\n");
    setenv("KAAPI_CUDA_KERNEL_PER_STREAM",getenv("XKBLAS_NKERNELS"),1);
  }

  if (getenv("XKBLAS_CACHE_LIMIT"))
    setenv("KAAPI_CUDA_CACHE_LIMIT",getenv("XKBLAS_CACHE_LIMIT"),1);

  const char* m = getenv("XKBLAS_DEFAULT_MATH");
  if (m !=0)
  {
    if ((strcasecmp(m,"TC") ==0)||(strcasecmp(m,"tensorcore") ==0)||(strcasecmp(m,"mix1632") ==0))
      xkblas_default_math = XKBLAS_TENSOR_OP_MATH;
    else if ((strcasecmp(m,"default") ==0)||(strcasecmp(m,"notc") ==0))
      xkblas_default_math = XKBLAS_DEFAULT_MATH;
    else
      printf("[XKBlas] unkown math mode '%s', use default\n", m);
  }
  xkblas_register_task_format();
  kaapi_register_format_writeback();
  kaapi_register_format_invalidate();
  kaapi_register_format_distribute();
  KAAPI_ATOMIC_WRITE(&_xkblas_thread_idx,0);
  kaapi_init();

  //_xkblas_global_team = kaapi_team_alloc();
#if XKBLAS_ADAPTATIVE
  if (getenv("XKBLAS_ADAPTIVE"))
  {
    if (atoi(getenv("XKBLAS_ADAPTIVE")) != 0)
      use_partition_thread_strategy = 1;
    else
      use_partition_thread_strategy = 0;
  }
#else
  if (getenv("XKBLAS_ADAPTIVE"))
    printf("***warning: XKBLAS_ADAPTIVE is defined by library not configured to support it\n");
#endif

  if (getenv("XKBLAS_VERBOSE"))
  {
    extern const char* get_kaapi_version(void);
    extern const char* get_kaapi_info(void);
    printf("[XKBlas info]\n%s%s[XKBlas info]\n", get_kaapi_info(), get_xkblas_info() );

    /* Some information about hierarchy
    */
    printf("#KAAPI_LD_NUMA=%i\n", kaapi_localitydomain_count(KAAPI_LD_NUMA) );

    if ((kaapi_localitydomain_count(KAAPI_LD_NUMA) != -1)
     && (kaapi_localitydomain_count(KAAPI_LD_NUMA) >0))
    {
      for (size_t i=0; i<kaapi_localitydomain_count(KAAPI_LD_NUMA); ++i)
        printf("  lid[%i]=%i\n", (int)i, (int)kaapi_localitydomain_get_num(KAAPI_LD_NUMA, i) );
    }
    printf("#KAAPI_LD_GPU=%i\n", kaapi_localitydomain_count(KAAPI_LD_GPU) );
    if ((kaapi_localitydomain_count(KAAPI_LD_GPU) != -1)
     && (kaapi_localitydomain_count(KAAPI_LD_GPU) >0))
    {
    for (size_t i=0; i<kaapi_localitydomain_count(KAAPI_LD_GPU); ++i)
      printf("  lid[%i]=%i, @ld:%p,  %s\n", 
          (int)i, (int)kaapi_localitydomain_get_num(KAAPI_LD_GPU, i), kaapi_localitydomain_get_bytype(KAAPI_LD_GPU, i), kaapi_localitydomain_info(KAAPI_LD_GPU, i) );
    }
  }

  /* */
  kaapi_thread_t* thread = xkblas_self_thread();
  kaapi_begin_dfg( thread, KAAPI_FRAME_FLAG_DFG_OK );

  return 0;
}


/*
*/
int xkblas_finalize(void)
{
  if (--init_count !=0) return 0;
  int err;

  /* TG: with several thread calling xkblas it is not possible to form the correct team
     only call end_dfg for local synchronisation
  */
  kaapi_end_dfg( _xkblas_self_context->kthread );
  kaapi_memory_synchronize();

  /* reset some global variable(s) to default value(s) */
  NB = 0;

  /* */
  kaapi_atomic_lock(&_xkblas_list_lock);

  int verbose=0;
  if (getenv("XKBLAS_VERBOSE"))
  {
    verbose = atoi(getenv("XKBLAS_VERBOSE"));
    if (verbose <0) verbose = 0;
  }
 
#if KAAPI_USE_PERFCOUNTER
  int disphead = 0;
  kaapi_offload_perfcounter_t cumul;
  char* task_names[KAAPI_FORMAT_MAX];
  if (verbose)
  {
    memset(&cumul, 0, sizeof(cumul));
    memset(&task_names, 0, sizeof(task_names));
    char tmp[12];
    for (int d=0; d<kaapi_offload_get_num_devices(); ++d)
    {
      kaapi_device_t* device = kaapi_offload_device(d);
      int dispdevice = 0;
      uint64_t spawn_count = 0;
      double time_count = 0, flops_count = 0;
      for (kaapi_format_id_t i=0; i<KAAPI_FORMAT_MAX; ++i)
      {
        if (device->perfcnt.task[i].spawn >0)
        {
          if (disphead ==0)
          {
            printf("[XKBlas stats]\n");
            disphead = 1;
          }
          if ((dispdevice ==0) && (verbose>=2))
          {
            printf("\t*device: %i\n", d);
            dispdevice =1;
          }
          kaapi_format_t* fmt = kaapi_format_resolve_byfmid(i);
          kaapi_format_get_name(fmt, 0, tmp, sizeof(tmp));
          task_names[i] = strdup(tmp);
          if (verbose >=2)
          {
            printf("\t[%12s]: count=%12li, time=%8e, flops=%10e, ai=%10e bar{ai}=%10e\n",
              tmp,
              device->perfcnt.task[i].spawn,
              device->perfcnt.task[i].time,
              device->perfcnt.task[i].flops,
              device->perfcnt.task[i].ai,
              device->perfcnt.task[i].ai/device->perfcnt.task[i].spawn
            );
          }
          spawn_count+= device->perfcnt.task[i].spawn;
          time_count+= device->perfcnt.task[i].time;
          flops_count+= device->perfcnt.task[i].flops;
  
          cumul.task[i].spawn += device->perfcnt.task[i].spawn;
          cumul.task[i].time += device->perfcnt.task[i].time;
          cumul.task[i].flops += device->perfcnt.task[i].flops;
          cumul.task[i].ai += device->perfcnt.task[i].ai;
        }
      }
      if (dispdevice)
      {
        printf("\t[%12s]: count=%12li, time=%8e, flops=%10e\n",
          "sum -->",
          spawn_count,
          time_count,
          flops_count
        );
      }
    }
    if (disphead)
    {
      printf("Resume for all devices\n");
      for (kaapi_format_id_t i=0; i<KAAPI_FORMAT_MAX; ++i)
      { 
        if (cumul.task[i].spawn >0)
        { 
          kaapi_format_t* fmt = kaapi_format_resolve_byfmid(i);
          printf("\t[%12s]: count=%12li, time=%8e, flops=%10e, ai=%10e bar{ai}=%10e\n",
            task_names[i],
            cumul.task[i].spawn,
            cumul.task[i].time,
            cumul.task[i].flops,
            cumul.task[i].ai,
            cumul.task[i].ai/cumul.task[i].spawn
          );
        }
      } 
      printf("[XKBlas stats]\n");
    }
  }
#endif

  while (_xkblas_list_context !=0)
  {
    xkblas_context_t* xkblas_ctxt = _xkblas_list_context;
    kaapi_team_deattach(xkblas_ctxt->kteam, xkblas_ctxt->kthread);
    kaapi_thread_unbind(xkblas_ctxt->kthread);
    kaapi_team_dealloc( xkblas_ctxt->kteam );
    xkblas_ctxt->kteam = 0;

    err = kaapi_hashmap_clear(&xkblas_ctxt->xkblas_ptr2handle);
    kaapi_assert(err ==0);
    err = kaapi_hashmap_destroy(&xkblas_ctxt->xkblas_ptr2handle);
    kaapi_assert(err ==0);

/* TG: comment. PPolet has detected bug because xkblas_ctxt->self may points to pthread' stack
  address already finalized.
    *xkblas_ctxt->self = 0;
*/
    _xkblas_list_context = xkblas_ctxt->next;
    free(xkblas_ctxt);
  }

  kaapi_atomic_unlock(&_xkblas_list_lock);

  /* can only doit for the main thread ! */
  _xkblas_self_context = 0;
  //kaapi_team_dealloc(_xkblas_global_team);
  //_xkblas_global_team = 0;

#if 00
  xkblas_free_curr_blochandle();
  while (freelist_blochandle !=0)
  {
    kaapi_stack_bloc_t* bloc = freelist_blochandle;
    freelist_blochandle = freelist_blochandle->next;
    kaapi_stackallocator_dealloc(&ctxt->st_allocator, bloc );
  }
#endif


  if (handle_cpublas != 0) 
    dlclose(handle_cpublas);
  handle_cpublas = 0;

  kaapi_finalize();
  pthread_key_delete( _pthread_xkblas_context_key );
  return 0;
}


/*
*/
#if KAAPI_DEBUG
size_t cnt_activated_handle = 0;
size_t count = 0;
size_t id_sync = 0;
#endif
int xkblas_sync(void)
{
#if defined(KAAPI_NVTX)
  nvtxRangePushA( __func__ );
#endif //defined(KAAPI_NVTX)
  xkblas_context_t* xk_ctxt = xkblas_context_get();
  kaapi_context_t* ctxt = kaapi_thread2context(xk_ctxt->kthread);
#if BUG_2022_03_18
printf("%p:: %30.30s , begin sync id: %lu, #handle: %i, #count activated: %i\n", pthread_self(), __FUNCTION__, id_sync, cnt_activated_handle, count);
#endif

  /* activate first synchronisation access */
  kaapi_handle_t* curr = xk_ctxt->xkblas_list_sync0;
  while (curr != 0)
  {
    kaapi_handle_t* next = (kaapi_handle_t*)curr->sync0.sync;
#if BUG_2022_03_18
printf("%p:: handle: %p, sync: %p\n", pthread_self(), curr, curr->sync);
#endif
    if (curr->sync != 0)
    {
#if KAAPI_DEBUG
      count += 
#endif
        kaapi_sched_activate_syncpoint(xk_ctxt->kthread, 0, &curr->sync0);
#if KAAPI_DEBUG
      ++cnt_activated_handle;
#endif
    }
    curr = next;
  }

//#undef KAAPI_DEBUG
//#define KAAPI_DEBUG 0

  /* also do sync */
  kaapi_end_dfg( xk_ctxt->kthread );
  kaapi_assert( xk_ctxt == xkblas_context_get() );

#define OLD_FLUSH 0
#if OLD_FLUSH // see xkblas_memory_invalidate_caches below
  xkblas_free_curr_blochandle();
  kaapi_hashmap_clear(&xk_ctxt->xkblas_ptr2handle);
  xk_ctxt->xkblas_list_sync0 = 0;
  xk_ctxt->xkblas_list_sync0_tail = 0;
#else
  /* reset all entries in ctxt->xkblas_list_sync0 */
  curr = xk_ctxt->xkblas_list_sync0;
  xk_ctxt->xkblas_list_sync0 = 0;
  xk_ctxt->xkblas_list_sync0_tail = 0;
  while (curr != 0)
  {
    kaapi_handle_t* next = (kaapi_handle_t*)curr->sync0.sync;
    kaapi_metadata_info_t* mdi = curr->mdi;
    if (mdi == 0)
      mdi = (curr->last->mode == KAAPI_ACCESS_SYNC ? 0: curr->last->mdi);
    kaapi_handle_init(xk_ctxt->kthread, curr, curr->sync0.data, mdi);

    curr->sync0.sync = (kaapi_access_t*)xk_ctxt->xkblas_list_sync0;
#if BUG_2022_03_18
printf("%p:: remain handle: %p, sync: %p\n", pthread_self(), curr, curr->sync);
#endif
    xk_ctxt->xkblas_list_sync0 = curr;
    if (xk_ctxt->xkblas_list_sync0_tail ==0)
      xk_ctxt->xkblas_list_sync0_tail = curr;
    curr = next;
  }
#endif

#if BUG_2022_03_18
printf("%p:: %30.30s , end sync id: %lu, #handle: %i, #count activated: %i\n\n\n", pthread_self(), __FUNCTION__, id_sync++, cnt_activated_handle, count);
#endif

  kaapi_begin_dfg( xk_ctxt->kthread, KAAPI_FRAME_FLAG_DFG_OK );
#if defined(KAAPI_NVTX)
  nvtxRangePop();
#endif //defined(KAAPI_NVTX)
  return 0;
}


/*
 */
int xkblas_memory_invalidate_caches(void)
{
#if BUG_2022_03_18
  printf("%p:: %30.30s\n", pthread_self(), __FUNCTION__);
#endif
  //kaapi_memory_invalidate_caches();
  xkblas_context_t* ctxt = xkblas_context_get();
#if OLD_FLUSH //see just above
  xkblas_free_curr_blochandle();
  kaapi_hashmap_clear(&_kblas_ptr2handle);
  ctxt->xkblas_list_sync0 = 0;
  ctxt->xkblas_list_sync0_tail = 0;
#else
  ++ctxt->xkblas_generation_cache;

  /* invalidate each block of matrix handle list */
  xkblas_matrix_descr_t* curr = ctxt->xkblas_matrix_descr_list;
  while (curr !=0)
  {
#if KAAPI_DEBUG
    kaapi_assert(curr->owner == pthread_self());
#endif
    if (xkblas_matrix_descr_isinit(curr))
    {
      size_t Amt = curr->mt;
      size_t Ant = curr->nt;

      for (size_t m = 0; m < Amt; ++m)
        for (size_t n = 0; n < Ant; ++n)
        {
          kaapi_handle_t* h = xkblas_get_handle(curr, m, n);
          kaapi_metadata_info_t* mdi = h->mdi;
          /* if data not touch, may be mdi ==0 */
          if (mdi !=0) 
            kaapi_memory_cache_invalidate_data( mdi );
        }
    }
    KAAPI_HASHENTRIES_SET((kaapi_hashentries_t*)curr->entry, 0, xkblas_matrix_descr_t*);
    xkblas_matrix_descr_t* ncurr = curr->next;
    free(curr);
    curr = ncurr;
  }

#if KAAPI_DEBUG
  /* here check all data in cache: it should not exist trace of data owned by the current thread */
  /* verify that their is no data with owner myself in all the caches */
  _kaapi_memory_cache_verify_notself();
#endif
  ctxt->xkblas_matrix_descr_list = 0;
  ctxt->xkblas_list_sync0 = 0;
  ctxt->xkblas_list_sync0_tail = 0;
#endif
  return 0;
}


/*
 */
int xkblas_memory_free(void)
{
  kaapi_memory_invalidate_caches();
  xkblas_context_t* ctxt = xkblas_context_get();
  /* free matrix handle list */
  while (ctxt->xkblas_matrix_descr_list !=0)
  {
    xkblas_matrix_descr_t* next = ctxt->xkblas_matrix_descr_list->next;
    free(ctxt->xkblas_matrix_descr_list->handle);
    free(ctxt->xkblas_matrix_descr_list);
    ctxt->xkblas_matrix_descr_list = next;
  }
  /* kaapi_handle_t are store in matrix descriptor */
  ctxt->xkblas_list_sync0 = 0;
  ctxt->xkblas_list_sync0_tail = 0;
  kaapi_hashmap_clear(&ctxt->xkblas_ptr2handle);

#if OLD_FLUSH //see just above
  xkblas_free_curr_blochandle();
  kaapi_hashmap_clear(&ctxt->xkblas_ptr2handle);
  ctxt->xkblas_list_sync0 = 0;
  ctxt->xkblas_list_sync0_tail = 0;
#endif
  return 0;
}

/*
*/
int xkblas_memory_syncall(void)
{
  //double t0 = kaapi_get_elapsedtime();
  xkblas_sync();
  kaapi_memory_synchronize();
  //double t1 = kaapi_get_elapsedtime();
  //printf("Time(s) sync + memory synchronise:%f\n",t1-t0);
  return 0;
}

/*
*/
int xkblas_memory_coherent_async(
  int uplo, int memflag,
  size_t M, size_t N,
  void* A, size_t LD, size_t eltsize
)
{
  xkblas_matrix_descr_t* Ah = xkblas_find(A);
  if (!xkblas_matrix_descr_isinit(Ah)) /* unknown matrix, return except if debug mode because it is strange to call this
  function with unknown matrix */
  {
    kaapi_assert_debug(xkblas_matrix_descr_isinit(Ah));
    return 0;
  }

  size_t A_MB = Ah->mb;
  size_t A_NB = Ah->nb;

  size_t LDA = LD;
  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;

{
  xkblas_context_t* ctxt = xkblas_context_get();
  kaapi_thread_t* thread = ctxt->kthread;
#if BUG_2022_03_18
printf("%p:: %30.30s: memflag: %i, thread: %p, A: %p\n", pthread_self(), __FUNCTION__, memflag, thread, A );
#endif
}

#if KAAPI_USE_TRACELIB==1
    kaapi_context_t* ctxt = kaapi_self_context();
    kaapi_event_t* evt = KAAPI_EVENT_GET(&ctxt->kproc, KAAPI_EVT_CALL, 0 /*begin*/ );
    if (evt)
    {
      strncpy(evt->u.s.d0.c8,"coherent",8);
      evt->u.s.d1.u = Ah->M;
      evt->u.s.d2.u = Ah->N;
      evt->u.s.d3.u = A_MB;
      KAAPI_EVENT_PUSH(&ctxt->kproc, KAAPI_EVT_CALL);
    }
    evt = KAAPI_EVENT_GET(&ctxt->kproc, KAAPI_EVT_CALL, 2 /*info*/ );
    if (evt)
    {
      evt->u.s.d0.u = A_NB;
      evt->u.s.d1.u = uplo;
      evt->u.s.d2.u = memflag;
      evt->u.s.d3.u = 0;
      KAAPI_EVENT_PUSH(&ctxt->kproc, KAAPI_EVT_CALL);
    }
#endif


  /* tile iteration */
  if (uplo ==0)
  {
    for (size_t m = 0; m < Amt; m++)
      for (size_t n = 0; n < Ant; n++)
        xkblas_create_taskwriteback( Ah, m, n, LD, eltsize );
  }
  else if (uplo == CblasUpper)
  {
    for (size_t m = 0; m < Amt; m++)
      for (size_t n = m; n < Ant; n++)
        xkblas_create_taskwriteback( Ah, m, n, LD, eltsize );

    /* invalidated the complementary part if memflag == 1*/
    if (memflag ==1)
    {
      for (size_t m = 0; m < Amt; m++)
        for (size_t n = 0; n < m; n++)
          xkblas_create_taskinvalidate( Ah, m, n, LD, eltsize );
    }
  }
  else if (uplo == CblasLower)
  {
    for (size_t m = 0; m < Amt; m++)
    {
      size_t min = m+1 < Ant ? m+1 : Ant;
      for (size_t n = 0; n < min; n++)
        xkblas_create_taskwriteback( Ah, m, n, LD, eltsize );
    }

    /* invalidated the complementary part if memflag == 1*/
    if (memflag ==1)
    {
      for (size_t m = 0; m < Amt; m++)
      {
        size_t Am = m == Amt-1 ? M - m*A_MB : A_MB;
        for (size_t n = m+1; n < Ant; n++)
          xkblas_create_taskinvalidate( Ah, m, n, LD, eltsize );
      }
    }
  }
  else
  {
    printf("[%s]: invalid argument uplo\n", __func__);
    abort();
  }

  return 0;
}


int xkblas_memory_invalidate_async(const void* A)
{
  // TODO
  return 0;
}



/* 2D data distribute
   store bloc (i,j) on a grid of ressource GpxGq (i/Bp)%Gp,(j/Bq)%Gq
   If matrix is not found return EINVAL
*/
int xkblas_distribute_2Dblock_cyclic_async(
  int hlevel, int storage, int uplo, size_t NB,
  size_t m, size_t n, const void* A, size_t lda, size_t eltsize,
  size_t Bp, size_t Bq, /* blocking size */
  size_t Gp, size_t Gq  /* grid size */
)
{
  xkblas_context_t* xkctxt = xkblas_context_get();
  
  xkblas_matrix_descr_t* Ah = xkblas_find(A);
  if (!xkblas_matrix_descr_isinit(Ah)) 
  {
    xkblas_init_matrix_handle(Ah, (void*)A, m, n, lda, eltsize, NB, NB);
  }
  
  kaapi_ld_type_t type;
  switch (hlevel) {
    case 0: type = KAAPI_LD_BOARD; break;
    case 1: type = KAAPI_LD_GPU; break;
    case 2: type = KAAPI_LD_CORE; break;
    default:
      printf("[%s] unknown type, returns immediatly\n", __func__);
#if KAAPI_DEBUG
      abort();
#endif
      return EINVAL;
  };
  unsigned int count = kaapi_localitydomain_count(type);
  if ((use_partition_thread_strategy) && (type ==KAAPI_LD_GPU))
    count = xkctxt->ngpus;

  if (count ==0) return EINVAL;

  size_t Amt = Ah->mt;
  size_t Ant = Ah->nt;
  char* ptr = (char*)A;
  void* addr;

  for (size_t i=0; i<Amt; ++i)
  {
    for (size_t j=0; j<Ant; ++j)
    {
      int r = ( ((i/Bp)%Gp)*Gq + (j/Bq)%Gq ) %count;
      if ((use_partition_thread_strategy) && (type ==KAAPI_LD_GPU))
      {
        int* gpuset = xkctxt->gpuset;
        kaapi_assert( r<xkctxt->ngpus );
        r = gpuset[r];
      }

      kaapi_ldid_t ldid = kaapi_localitydomain_get_num(type, r);
      xkblas_create_distribute( Ah, i, j, lda, eltsize, ldid );
      xkblas_set_ldid(Ah, i, j, ldid );
    }
  }
  return 0;
}


/* 1D distribution of tiles
   store bloc [i,.] on a ressource (i/B)%G
   colrow = 0 -> col mapping
   colrow = 1 -> row mapping
*/
int xkblas_distribute_1Dblock_cyclic_async(
  int hlevel, int storage, int colrow, int uplo, size_t NB, 
  size_t m, size_t n, const void* A, size_t lda, size_t eltsize,
  size_t B, size_t G    /* grid size */
)
{
  size_t MB;
  MB = NB;
  if (colrow == 0)
  {
    return xkblas_distribute_2Dblock_cyclic_async(
      hlevel, storage, uplo, NB, m, n, A, lda, eltsize,
      (m+MB-1)/MB, B,
      1, G
    );
  }
  else
  {
    return xkblas_distribute_2Dblock_cyclic_async(
      hlevel, storage, uplo, NB, m, n, A, lda, eltsize,
      B, (n+NB-1)/NB,
      G, 1
    );
  }
  return EINVAL;
}


/* FLOPS per kernel. TODO: add side
*/
double _xkblas_get_cost( double* data, xkblas_kernel_t kernel, size_t M, size_t N, size_t K )
{ 
  double flops = 0.0;

  switch (kernel) {
    case KERN_HERK:
      *data = DATA_ZHERK(M,N);
      flops = FLOPS_ZHERK(M, N);
//printf("%s:: K=%i, N=%i, Flops HERK=%g\n", __func__, M, N, flops);
    break;
    case KERN_SYRK:
      *data = DATA_ZSYRK(M, N);
      flops = FLOPS_ZSYRK(M, N);
//printf("%s:: K=%i, N=%i, Flops SYRK=%g\n", __func__, M, N, flops);
    break;
    case KERN_HER2K:
      *data = DATA_ZHER2K(M, N);
      flops = FLOPS_ZHER2K(M, N);
//printf("%s:: K=%i, N=%i, Flops HER2K=%g\n", __func__, M, N, flops);
    break;
    case KERN_SYR2K:
      *data = DATA_ZSYR2K(M, N);
      flops = FLOPS_ZSYR2K(M, N);
//printf("%s:: K=%i, N=%i, Flops SYR2K=%g\n", __func__, M, M, flops);
    break;
    case KERN_HEMM:
      *data = DATA_ZHEMM(CblasLeft, M, N);
      flops = FLOPS_ZHEMM(CblasLeft, M, N);
//printf("%s:: M=%i, N=%i, Flops HEMM=%g\n", __func__, M, N, flops);
    break;
    case KERN_SYMM:
      *data = DATA_ZSYMM(CblasLeft, M, N);
      flops = FLOPS_ZSYMM(CblasLeft, M, N);
//printf("%s:: M=%i, N=%i, Flops SYMM=%g\n", __func__, M, N, flops);
    break;
    case KERN_TRMM:
      *data = DATA_ZTRMM(CblasLeft, M, N);
      flops = FLOPS_ZTRMM(CblasLeft, M, N);
//printf("%s:: M=%i, N=%i, Flops TRMM=%g\n", __func__, M, N, flops);
    break;
    case KERN_TRSM:
      *data = DATA_ZTRSM(CblasLeft, M, N);
      flops = FLOPS_ZTRSM(CblasLeft, M, N);
//printf("%s:: M=%i, N=%i, Flops TRSM=%g\n", __func__, M, N, flops);
    break;
    case KERN_GEMMT:
      *data = DATA_ZGEMMT(N,K);
      flops = FLOPS_ZGEMMT(N,K);
//printf("%s:: M=%i, N=%i, K=%i, Flops GEMMT=%g\n", __func__, M, N, K, flops);
    break;
    case KERN_GEMM:
      *data = DATA_ZGEMM(M, N,K);
      flops = FLOPS_ZGEMM(M, N,K);
//printf("%s:: M=%i, N=%i, K=%i, Flops GEMM=%g\n", __func__, M, N, K, flops);
    break;
    case KERN_SWAP:
    case KERN_COPYSCALE:
      *data = DATA_MAT(M,N);
      flops = 3*(double)M*(double)N;
    break;
    default: 
      return 0;
    break;
  }
  return flops;
}



static inline int min(int a, int b) { return a<b ? a:b; }

/* Return the bloc size:
   - M,N should be the dimension of the result. K depends of the Kernel.
   Return NB such that (M,N) is computed in FACTOR*NGPU blocs
*/
size_t xkblas_auto_tilesize(
  xkblas_context_t* xkctxt, xkblas_kernel_t kernel, size_t M, size_t N, size_t K
)
{
  /* get default tile size and initialize internal descriptor if not yet */
  size_t NB = xkctxt->NB;
  int SW_ngpus= kaapi_localitydomain_count(KAAPI_LD_GPU); /* system wide number of GPUs (on the current node)*/
  size_t ngpus = xkblas_get_ngpus(); 
  size_t fact = 2;
  size_t minNB = 2048; 
  int force_todefault_mapping = 1;

#if XKBLAS_ADAPTATIVE==1
  double data;
  double W = _xkblas_get_cost(&data, kernel, M, N, K);
  double Wt = 0.5*(double)min(NB,M)*(double)min(NB,N)*(double)min(NB,K);
  double D = fact*(double)NB*(double)NB;
  int cntzero; /* number of inactive GPUs */
#endif

  if (use_partition_thread_strategy ==0)
  {
    force_todefault_mapping = 1;
    if (NB !=0)
    {
#if 0
      _kaapi_lock_print();
      printf("%s:: NB fixed to:%i, #GPUS=%i M:%i, N:%i, K:%i\n", __func__, NB, xkctxt->ngpus, M,N,K);
      _kaapi_unlock_print();
#endif
      return NB;
    }
  }
  else
  { /* use or defined xkctxt->ngpus & xkctxt->gpuset
       Current prototype only detec concurrent BLAS call if xkblas thread are in a parallel OpenMP region
    */
#if XKBLAS_ADAPTATIVE==0
    /* not configure, so this case could never occurs */
    kaapi_assert(use_partition_thread_strategy ==0);
#else
    if (omp_get_num_threads() >1)
    {
      force_todefault_mapping = 0;
      fact = 1;

      int self = xkctxt->kctxt->tid % SW_ngpus;
      xkctxt->ngpus = 1;
      xkctxt->gpuset[0] = self;

      float load[SW_ngpus];
      int izero[SW_ngpus];
      int imax[KAAPI_IMAX];
      float max;
      float min;
      float avrg;
      float delta;
      int iimax = _kaapi_compute_load_device(&min, &max, &avrg, &delta, imax, &cntzero, izero, load);
      float minmax = max-min;
#if 0
      _kaapi_lock_print();
      printf("%s:: Below L0:: M=%i, N=%i, K=%i LoadAvrg=%g LoadMax=%g CntZero=%i, Load=", __func__, M, N, K, avrg, max, cntzero );
      for (int i=0; i<SW_ngpus; ++i)
        printf("%g ", load[i]);
      printf("\n");
      _kaapi_unlock_print();
#endif
#if 1
      size_t minNB = 2048; 
      force_todefault_mapping = 0;
      if (NB==0) NB = minNB;
      fact =1;

      //double Pavrg = W/D; /* 1GPU max */
      xkctxt->ngpus = cntzero; // (Pavrg < cntzero ? (int)Pavrg : cntzero);
      if (xkctxt->ngpus==0) { xkctxt->ngpus = 1; }
      if (cntzero >0)
      {
        int j = 1; /* self is already at position 0 */
        for (int i=0; i<xkctxt->ngpus; ++i) 
        {
          if (izero[i] != self)
          {
            kaapi_assert_debug( izero[i] < SW_ngpus );
            xkctxt->gpuset[j++] = izero[i];
          }
        }
      }
#else
      /* number of worker is :omp_get_num_threads() */
      if (cntzero > SW_ngpus/2)
      {
        force_todefault_mapping = 1;
        xkctxt->ngpus = (cntzero > SW_ngpus*2/3 ? (cntzero < SW_ngpus ? cntzero : SW_ngpus) : cntzero/2);
        if (xkctxt->ngpus==0) { xkctxt->ngpus = 1; }
        int j = 1; /* self is already at position 0 */
        for (int i=0; i<xkctxt->ngpus; ++i) 
        {
          if (izero[i] != self)
          {
            kaapi_assert_debug( izero[i] < SW_ngpus );
            xkctxt->gpuset[j++] = izero[i];
          }
        }
        minNB *=1;

#if 1
        if (NB !=0)
        {
          return 2*NB;
        }
#endif
      }
      else /* favor coarse grain local submission with pipelining comm */
      {
	minNB *=2;
        if (NB !=0)
          return NB*2;
      }
#endif // end if 1
    }
    else {
      // In // OpenMP region 
#if 1// 
      force_todefault_mapping = 0;
      size_t minNB = 2048; //xkctxt->NB; //1024; 
      if (NB==0) NB = minNB;
      fact =1;
      double Pavrg = W/D; /* 1GPU max */
      xkctxt->ngpus = (Pavrg < SW_ngpus ? (int)Pavrg : SW_ngpus);
      kaapi_assert( xkctxt->ngpus < XKBLAS_MAX_NGPUS);
      for (int i=0; i<xkctxt->ngpus; ++i)
        xkctxt->gpuset[i] = i;
#else
      force_todefault_mapping = 1;
      fact = 2;
      xkctxt->ngpus  = SW_ngpus;
      kaapi_assert( xkctxt->ngpus < XKBLAS_MAX_NGPUS);
      for (int i=0; i<xkctxt->ngpus; ++i)
        xkctxt->gpuset[i] = i;
      if (NB !=0)
      {
        return NB;
      }
#endif
    }
#endif
  }

  if (force_todefault_mapping ==0)
  {
    if (N>M) M =N;
    switch (kernel)
    {
      case KERN_HERK:
      case KERN_SYRK:
      case KERN_HER2K:
      case KERN_SYR2K:
      case KERN_HEMM:
      case KERN_SYMM:
      case KERN_TRMM:
      case KERN_TRSM:
      case KERN_GEMMT:
      case KERN_GEMM:
      case KERN_SWAP:
      case KERN_COPYSCALE:
      default:
        {
          /* take max */
#if 1
          if (omp_get_num_threads() >1)
          {
            double ffact = 4; // fact
            double k =  W/(ffact * (double)xkctxt->ngpus);
            double fNB = pow(k,1.0/3.0);
	    size_t cntb  = ceil(M/fNB);
            NB = M/cntb;
            //if ((omp_get_num_threads() >1) && (NB > minNB)) NB = minNB;
            if (NB > 8192) NB = 8192; /* on MI50, TRSM has an error due to insufficient memory allocation */
          }
          else {
#  if 0
            NB = M / (fact*xkctxt->ngpus);
            NB = (NB + 63) & ~63UL;
            if (NB <minNB) NB = minNB;
#  else
            double ffact = 4;
            double fNB = sqrt( (double)M*(double)N / (ffact * (double)(xkctxt->ngpus * kaapi_default_param.cuda_conc_stream_kernel)) );
            NB = ceil(fNB);
#  endif
         }
#else
          NB = M / (fact*xkctxt->ngpus);
          NB = (NB + 63) & ~63UL;
#endif
          if (NB <minNB) NB = minNB;
#if 0
printf("%s::%s L0: (M,N,K)=(%llu, %llu, %llu), (W,Wt,D,Pavrg)=(%g,%g,%g,%g) => NB =%llu / NGPU=%i (%i,...)\n", 
    __func__,  (omp_get_num_threads() >1 ? "Under" : "Above"),
    M,N,K, W,Wt,D,W/D, NB, xkctxt->ngpus, xkctxt->gpuset[0]);
#endif
        }
        break;
    }
  }
  else /* force_todefault_mapping == 1 */
  {
    if (N>M) M =N;
    switch (kernel)
    {
      case KERN_HERK:
      case KERN_SYRK:
        {
          fact = 2;
  redo_syrk:
          NB = M / (fact*xkctxt->ngpus);
          if (NB >= 2048)
            NB= NB & ~2047UL;
          if ((NB <= 1024) && (fact==2))
          { fact=1; goto redo_syrk;}
          if (NB >4096) NB = M / (2*fact*xkctxt->ngpus);
          if (NB <512) NB = 512;
        }
        break;

      case KERN_HER2K:
      case KERN_SYR2K:
        {
          fact = 4;
  redo_syr2k:
          NB = M / (fact*xkctxt->ngpus);
          NB= (NB +63UL)& ~63UL;
          if ((NB <= 2048) && (fact>2))
          { --fact; goto redo_syr2k;}
          if (NB >4096) NB = M / (2*fact*xkctxt->ngpus);
          if (NB <512) NB = 512;
        }
        break;

      case KERN_HEMM:
      case KERN_SYMM:
        {
          fact = 1;
          NB = M / (fact*xkctxt->ngpus);
          NB= NB & ~1023UL;
          if (NB <1024) NB = 1024;
          if (NB <4096) NB *= 2;
        }
        break;

      case KERN_TRMM:
      case KERN_TRSM:
  //    case KERN_GEMMT:
  //    case KERN_GEMM:
        {
  //        fact = 2;
          NB = M / (fact*xkctxt->ngpus);
          NB = (NB + 63) & ~63UL;
          if (NB <minNB) NB = minNB;
        }
        break;

#if 1 
      case KERN_GEMMT:
      case KERN_GEMM:
        {
          fact = 1;
          NB = M / (fact*xkctxt->ngpus);
          if (M >xkctxt->ngpus)
          {
            if ((M> 16384)&&(NB<2048)) NB=2048;
          }
          if (NB >4096) NB = M / (2*fact*xkctxt->ngpus);
          if (NB <512) NB = 512;
          NB = (NB + 63) & ~63UL;
        }
        break;
#endif

      case KERN_SWAP:
      case KERN_COPYSCALE: /* TODO ?*/
      default:
        {
          fact = 1;
          NB = M / (fact*xkctxt->ngpus);
          if (NB >=4096) NB = M / (2*fact*xkctxt->ngpus);
          if (NB <1024) NB = 1024;
          NB = (NB + 63) & ~63UL;
        }
        break;
    }
  }

#if 0
  if (force_todefault_mapping==0)
    printf("%s:: Under L0: #GPUS=%i, GPU[0]:%i, M:%i, N:%i, K:%i -> NB=%i\n", __func__, xkctxt->ngpus, xkctxt->gpuset[0], M,N,K,NB);
  else
    printf("%s:: #GPUS=%i, M:%i, N:%i, K:%i -> NB=%i\n", __func__, xkctxt->ngpus, M,N,K,NB);
#endif
  return NB;
}


/* Map tile of matrix descriptor.
   Hardcoded selection between 1D or 2D mapping depending of the type of kernel
*/
kaapi_atomic_t count_call ={0};
int xkblas_auto_map(
  xkblas_context_t* xkctxt,
  xkblas_kernel_t kernel,
  xkblas_matrix_descr_t* Ah
)
{
  size_t ngpus = xkctxt->ngpus;
  if (ngpus ==0) return EBUSY;

  switch (kernel)
  {
    case KERN_SYRK:
#if 0
    {
      xkblas_map_ij_cyclic(xkctxt, 
        1, CblasColMajor,
        Ah->M, Ah->N, Ah->addr, Ah->ld, Ah->eltsize,
        0
      );
    } break;
#endif

    case KERN_COPYSCALE:
    case KERN_SYR2K:
    case KERN_SYMM:
    case KERN_GEMM:
    case KERN_TRMM:
    case KERN_GEMMT:
    case KERN_TRSM:
    case KERN_HEMM:
    case KERN_HERK:
    case KERN_HER2K:
    { 
#define EXP_LO 0
#if EXP_L0 // experimental
      int underloaded = 0;
      int iimin = 0;
      int GPU=0;
      float sum = 0.0;
      { /* compute #ngpu underloaded and the index of gpu that has minimal load */
        int ngpu= kaapi_localitydomain_count(KAAPI_LD_GPU);
        float load[ngpu];
        int min = INT_MAX;
        int iimin = 0;
        for (int i=0; i<ngpu; ++i)
        {
          kaapi_localitydomain_t* ld = kaapi_localitydomain_get_bytype(KAAPI_LD_GPU,i);
          if (ld !=0)
          {
            load[i] = ld->device->pendingtasks;
            //load[i] = ld->device->flops_tasks;
            sum += (float)load[i];
            int l = load[i];
            if (l < min)
            {
              min = l;
              iimin = i;
            }
          }
        }
        sum /= (float)ngpu;
        for (int i=0; i<ngpu; ++i)
        {
           if (load[i] <= 0.5*sum) ++underloaded;
        }
      }
      int dispatch = (underloaded >= 0.5*ngpu); //dispatch =0; (Ah->mt*Ah->nt > 32* ngpu); 
      if (dispatch ==0) GPU = iimin; //GPU= KAAPI_ATOMIC_INCR(&count_call);
 printf("%i:: [dispatch] kernel: %i, avrg load: %f, dispatch: %i, iff==0, GPU= %i\n", ctxt->kctxt->kid, kernel, sum, dispatch, GPU);
      //printf("%s:: tid: %i, kid: %lu => dispatch: %i\n", __FUNCTION__, ctxt->kctxt->tid, ctxt->kctxt->kid, dispatch);
      if (dispatch)
#endif
      {
        /* 2D Bloc cyclic */
        size_t Blkm = 1;
        size_t Blkn = 1;
        size_t Gm = sqrt(ngpus);
        size_t Gn = Gm;
        if (Gm ==0) { Gn = ngpus; Gm = 1; }
        else {
          /* find the most square decomposition of ngpus in Gm x Gn */
          size_t g;
          for (g = Gm+1; g>0; --g)
             if (ngpus % g == 0) break;
          if (g==0) { Gm = ngpus; Gn = 1; }
          //if (g==0) { Gm = 1; Gn = ngpus; }
          else { Gm = g; Gn = ngpus/g; }
        }
        //printf("Block2D cyclic: Blkm: %i, Blkn: %i, Gm: %i, Gn: %i\n", Blkm, Blkn, Gm, Gn);
        xkblas_map_2Dblock_cyclic(
          xkctxt,
          1, CblasColMajor,
          Ah->M, Ah->N, Ah->addr, Ah->ld, Ah->eltsize,
          Blkm, Blkn, Gm, Gn, 0
        );
      }
#if EXP_LO
      else 
      {
        //xkblas_map_2Dblock_cyclic(
        //  1, CblasColMajor,
        //  Ah->M, Ah->N, Ah->addr, Ah->ld, Ah->eltsize,
        //  Blkm, Blkn, Gm, Gn, 0
        //);
        xkblas_map_all(
          xkctxt,
          1, CblasColMajor,
          Ah->M, Ah->N, Ah->addr, Ah->ld, Ah->eltsize,
          GPU % ngpus, 0
        );
       }
#endif
    } break;

    case KERN_SWAP:
      break;
    case KERN_VOID:
      break;
  }
  return 0;
}



/* kaapi_get_elapsedtime
*/
uint64_t xkblas_elapsedns(void)
{
  return kaapi_get_elapsedns();
}

/* kaapi_get_elapsedns
*/
double xkblas_elapsedtime(void)
{
  return kaapi_get_elapsedtime();
}

/* Load symbol in the sub cpu BLAS library
*/
extern void xkblas_load_sym(void** ptr, const char* name)
{
  if (handle_cpublas ==0)
  {
    handle_cpublas = dlopen(XKBLAS_BLASLIB,RTLD_LAZY);
    if (handle_cpublas ==0)
    {
      printf("[xkblas]: cannot load liblas '%s'\n", XKBLAS_BLASLIB);
      abort();
    }
  }
  *ptr = dlsym( handle_cpublas, name );
  if (*ptr ==0)
  {
    fprintf(stderr,"*** Error: [xkblas] cannot load symbol '%s' from '%s'\n", name, XKBLAS_BLASLIB);
    abort();
  }
}


/* How it was compiled */
const char* get_xkblas_info(void)
{ 
  static char buffer[8192];
  static int isinit = 0;
  if (isinit ==0)
    snprintf( buffer, 8192,
            "  GPU PART.: %s\n"
            "  TILE_SIZE: %lu\n"
            "  MODE_MATH: %s\n",
         (use_partition_thread_strategy ==0 ? "NONE" : "THREAD"),
         xkblas_get_param(),
         (xkblas_default_math == XKBLAS_TENSOR_OP_MATH ? "TensorCore" : "Default")
    );
  return buffer; 
}

/*
 * Unified work buffer implementation (should be moved in kaapi)
 */
pthread_mutex_t work_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  work_buffer_cond  = PTHREAD_COND_INITIALIZER;
struct memory_segment {
	size_t    size;
	int       state;
	uintptr_t ptr;
	struct memory_segment* prev;
	struct memory_segment* next;
	struct memory_segment* free_next;
	struct memory_segment* free_prev;
};
#define MAIN_STATE 0x2
#define FREE_STATE 0x1

struct memory_segment* free_segment_list = NULL;
// I think the first_segment will never be removed => this is ok
struct memory_segment* first_segment = NULL;
size_t total_size = 0;

//#define DEBUG_SEG 1

void coherency_check()
{
	struct memory_segment* first_free = NULL;
	struct memory_segment* curr = first_segment;
	while( curr )
	{
		// Common
		//size_t    size;
		//int       state;
		if( curr->prev != NULL )
		{
			if( curr->prev->ptr >= curr->ptr )
			{
				printf("[SEG][%p] %p invalid prev ptr\n\n\n\n", pthread_self(), curr);
				exit(1);
			}
			if( curr->prev->next != curr )
			{
				printf("[SEG][%p] %p invalid prev next\n\n\n\n", pthread_self(), curr);
				exit(1);
			}
		}	
		else
		{
			if( curr->free_prev != NULL )
			{
				printf("[SEG][%p] %p invalid free prev after no next\n\n\n\n", pthread_self(), curr);
				exit(1);
			}
		}
		if( curr->next != NULL )
		{
			if( curr->next->ptr <= curr->ptr )
			{
				printf("[SEG][%p] %p invalid next ptr\n\n\n\n", pthread_self(), curr);
				exit(1);
			}
			if( curr->next->prev != curr )
			{
				printf("[SEG][%p] %p invalid next prev\n\n\n\n", pthread_self(), curr);
				exit(1);
			}
		}
		else
		{
			if( curr->free_next != NULL )
			{
				printf("[SEG][%p] %p invalid free next after no next\n\n\n\n", pthread_self(), curr);
				exit(1);
			}
		}

		if( curr->state & FREE_STATE )
		{ // Free segment
			if( curr->free_prev != NULL )
			{
				if( curr->free_prev->ptr >= curr->ptr )
				{
					printf("[SEG][%p] %p invalid free prev ptr\n\n\n\n", pthread_self(), curr);
					exit(1);
				}
				if( curr->free_prev->free_next != curr )
				{
					printf("[SEG][%p] %p invalid free prev next\n\n\n\n", pthread_self(), curr);
					exit(1);
				}
			}
			else
			{
				if( curr != free_segment_list )
				{
					printf("[SEG][%p] %p invalid free segment list\n\n\n\n", pthread_self(), curr);
					exit(1);
				}
			}	
			if( curr->free_next != NULL )
			{
				if( curr->free_next->ptr <= curr->ptr )
				{
					printf("[SEG][%p] %p invalid free next ptr\n\n\n\n", pthread_self(), curr);
					exit(1);
				}
				if( curr->free_next->free_prev != curr )
				{
					printf("[SEG][%p] %p invalid free next prev\n\n\n\n", pthread_self(), curr);
					exit(1);
				}
			}
			
		}
		else
		{ // Non-free segment
			if( curr->free_prev != NULL )
			{
				printf("[SEG][%p] %p invalid free prev\n\n\n\n", pthread_self(), curr);
				exit(1);
			}	
			if( curr->free_next != NULL )
			{
				printf("[SEG][%p] %p invalid free next\n\n\n\n", pthread_self(), curr);
				exit(1);
			}
		}
	
		curr = curr->next;
	}
	printf("[SEG][%p] Coherency check ok\n", pthread_self());
}

void print_segments()
{
	printf("[SEG][%p] free_start %p\n", pthread_self(), free_segment_list);
	struct memory_segment* curr = first_segment;
	while( curr )
	{
		printf("[SEG][%p]   seg: %p, %p[%p] -> %p (%p,%p,%p), %d\n", pthread_self(), curr, curr->ptr, curr->size, curr->next, curr->prev, curr->free_prev, curr->free_next, curr->state);
		curr = curr->next;
	}
	coherency_check();
}

void* xkblas_get_work_pos(size_t size)
{
	//printf("try alloc %lx\n", size);
	if( size == 0 )
		return NULL;
	size = (size + 7UL) & ~7UL; // Align
				    
	if(total_size < size)
	{ // Needed size is greater than available one
		printf("[XKBLAS] needed size %ld is greater than available one %ld\n", size, total_size);
		exit(1);
	}

	//printf("[LOC][%p] xkblas_mutex_lock %p (alloc)\n", pthread_self(), &work_buffer_mutex);
	pthread_mutex_lock( &work_buffer_mutex );	
	//printf("[LOC][%p] xkblas_mutex_lock %p (alloc) -- locked\n", pthread_self(), &work_buffer_mutex);
	while(1)
	{ // We wait until data are available
		struct memory_segment* curr = free_segment_list;
	  	while(curr)
		{
			//printf("Freelist: %p size %lx\n", curr, curr->size);
			size_t curr_size = curr->size;
			if( curr_size >= size )
			{ // We have enought space to work here
				if( curr_size - size > 0 )
				{
					// create remainder
					struct memory_segment* remainder = malloc(sizeof(struct memory_segment));
					remainder->ptr  = curr->ptr + size;
					remainder->size = curr_size - size;
				        remainder->state = FREE_STATE;
					remainder->prev = curr;
					remainder->next = curr->next;
					if(curr->next) curr->next->prev = remainder;
					remainder->free_next = curr->free_next;
					remainder->free_prev = curr;
					if(remainder->free_next) remainder->free_next->free_prev = remainder;

					// update curr
					curr->next = remainder;
					curr->size = size;
					curr->free_next = remainder;
				}
				break;
			}
			curr = curr->free_next;	
		}
		if(curr)
		{ // We got a valid free segment, we need to update his status
			curr->state &= ~FREE_STATE;
			if(curr->free_next) curr->free_next->free_prev = curr->free_prev;
			if(curr->free_prev) curr->free_prev->free_next = curr->free_next;
			else free_segment_list = curr->free_next; // In this case, curr is the first free segment
			curr->free_next = NULL;
			curr->free_prev = NULL;
#if defined(DEBUG_SEG)
			printf("[SEG][%p] allocate %p\n", pthread_self(), (void*) curr->ptr);
			print_segments();
			printf("[SEG]\n");
#endif
			//printf("[LOC][%p] xkblas_mutex_unlock %p\n", pthread_self(), &work_buffer_mutex);
			pthread_mutex_unlock( &work_buffer_mutex );
			//printf("allocate %p\n", (void*) curr->ptr);
			return (void*) curr->ptr;	
		}
		//printf("[LOC][%p] xkblas_cond_wait %p,%p\n", pthread_self(), &work_buffer_cond, &work_buffer_mutex);
		pthread_cond_wait( &work_buffer_cond, &work_buffer_mutex );
		//printf("[LOC][%p] xkblas_cond_wait %p,%p -- liberated\n", pthread_self(), &work_buffer_cond, &work_buffer_mutex);
	}
}

void xkblas_free_work_pos( void* ptr )
{
	//printf("try free %p\n", ptr);
	//printf("[LOC][%p] xkblas_mutex_lock %p (free)\n", pthread_self(), &work_buffer_mutex);
	pthread_mutex_lock( &work_buffer_mutex );
	//printf("[LOC][%p] xkblas_mutex_lock %p (free) -- locked\n", pthread_self(), &work_buffer_mutex);
	
#if defined(DEBUG_SEG)
	printf("[SEG][%p] will free %p\n", pthread_self(), ptr);
	print_segments();
#endif

	// Step 1: find the associated segment
	struct memory_segment* curr = first_segment;
	while(curr)
	{
		if(curr->ptr == (uintptr_t) ptr)
		{
			break;
		}
		if((uintptr_t) curr->ptr > ptr)
		{
			curr = NULL;
			break;
		}
		curr = curr->next;
	}
	if(curr == NULL || (curr->state & FREE_STATE))
	{ // curr does not exist or is already free
	        printf("[SEG][%p] Curr err %p\n", pthread_self(), curr);
		exit(1); // TODO clean this
	}

	// Step 2: free it
	curr->state = FREE_STATE;

	int inserted = 0;

	struct memory_segment* next_seg = curr->next;
	if( next_seg && (next_seg->state & FREE_STATE) )
	{ // We should merge -> prev is always conserved
		curr->size += next_seg->size;
		curr->next = next_seg->next;
		if(curr->next) curr->next->prev = curr;

		curr->free_next = next_seg->free_next;
		if(curr->free_next) curr->free_next->free_prev = curr;
		curr->free_prev = next_seg->free_prev;
		if(curr->free_prev) curr->free_prev->free_next = curr;
		else free_segment_list = curr;

		free(next_seg);

		inserted = 1;	
	}

	struct memory_segment* prev_seg = curr->prev;
	if( prev_seg && (prev_seg->state & FREE_STATE) )
	{ // We should merge -> prev is always conserved
		prev_seg->size += curr->size;
		prev_seg->next = curr->next;
		if(prev_seg->next) prev_seg->next->prev = prev_seg;

		if( inserted == 1 )
		{
			prev_seg->free_next = curr->free_next;
			if(prev_seg->free_next) prev_seg->free_next->free_prev = prev_seg;
		}

		free(curr);

		inserted = 1;
	}

	if( inserted == 0 )
	{
		if( free_segment_list == NULL )
		{
			free_segment_list = curr; // No free segments available
		}
		else
		{
			struct memory_segment* last_free_curr = NULL;
			struct memory_segment* free_curr = free_segment_list;
			while( free_curr )
			{
				if(free_curr->ptr > curr->ptr)
					break;
				last_free_curr = free_curr;
				free_curr = free_curr->free_next;
			}

			if( last_free_curr == NULL )
			{ // We have the first free segment
				curr->free_next = free_segment_list;
				curr->free_prev = NULL;
				free_segment_list = curr;
			}
			else
			{
				curr->free_prev = last_free_curr;
				curr->free_next = free_curr;
			}
			if(curr->free_prev) curr->free_prev->free_next = curr;
			if(curr->free_next) curr->free_next->free_prev = curr;
		}
		//if( free_curr == NULL )
		//{ // We can't found a free segment after this one
		//	if( last_free_curr == NULL )
		//	{ // their is not free segment (should not happen)
		//		free_segment_list = curr;
		//	}
		//	else
		//	{
		//	}
		//}
	}
	
	//printf("[LOC][%p] xkblas_cond_broadcast %p\n", pthread_self(), &work_buffer_cond);
	pthread_cond_broadcast( &work_buffer_cond );
	//printf("[LOC][%p] xkblas_mutex_unlock %p\n", pthread_self(), &work_buffer_mutex);
#if defined(DEBUG_SEG)
	printf("[SEG][%p] After free:\n", pthread_self());
	print_segments();
	printf("[SEG] \n");
#endif
	pthread_mutex_unlock( &work_buffer_mutex );
}

struct memory_buffer_args {
	void* pos;
	size_t size;
	size_t element_size;
};

void* memset_init_thread_function(void* args)
{
	struct memory_buffer_args* bargs = (struct memory_buffer_args*) args;

#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif

	uintptr_t pos = (uintptr_t) bargs->pos;
	//printf("range [%p,%p[\n", pos, pos + bargs->size );
	size_t keep = bargs->size;
	while( pos < ((uintptr_t) bargs->pos) + bargs->size )
	{
		size_t min = (keep < bargs->element_size) ? keep : bargs->element_size;
		printf("memset %x at [%p,%p[ | %x/%x\n", min, pos, pos + min, keep, bargs->size);
		driver->f_memset( (void*) pos, 0, min );
		xkblas_free_work_pos( (void*) pos );
		pos += bargs->element_size;
		keep -= bargs->element_size;
	}
	free( args );
	return NULL;
}	
void xkblas_register_work_buffer( void* ptr, size_t size )
{
	pthread_mutex_lock( &work_buffer_mutex );
	if( first_segment != NULL )
	{ // Something is already registered
		exit(1);	
	}

	total_size = size;
	struct memory_segment* seg = (struct memory_segment*) malloc( sizeof(struct memory_segment) );
	seg->size  = size;
	seg->state = FREE_STATE;
	seg->ptr   = (uintptr_t) ptr;
	seg->prev  = NULL;
	seg->next  = NULL;
	seg->free_next = NULL;
	seg->free_prev = NULL;

	free_segment_list = seg;
	first_segment = seg;
	pthread_mutex_unlock( &work_buffer_mutex );

#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif
	printf("set gpu work %ld\n", size);
	driver->f_advise_gpu( ptr, size);
	printf("advise ok\n");
	
	struct memory_buffer_args *bargs = (struct memory_buffer_args*) malloc(sizeof(struct memory_buffer_args));
	bargs->pos =  (uintptr_t) ptr;
	bargs->size = size;
	bargs->element_size = 1024L * 1024L * 1024L;

	printf("Start alloc all segs\n");
	uintptr_t pos = (uintptr_t) bargs->pos;
	size_t total_size = 0;
	size_t keep = bargs->size;
	while( pos < ((uintptr_t) bargs->pos) + size )
	{
		size_t min = (keep < bargs->element_size) ? keep : bargs->element_size;
		void* get_ptr = xkblas_get_work_pos(min);
//		printf("%p %p %p %lx/%lx\n", (void*) pos, get_ptr, (void*) ((uintptr_t) bargs->pos) + size, total_size, size );	

		pos += min;
		total_size += min;
		keep -= min;
	}
	printf("End alloc all segs\n");

	pthread_t thread;
	pthread_create( &thread, NULL, memset_init_thread_function, bargs );
	pthread_join( thread, NULL );	
	//driver->f_memset( ptr, 0, size );
}




void xkblas_bind_cpu( void* ptr, size_t size )
{
#if KAAPI_USE_CUDA
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_CUDA );
#endif
#if KAAPI_USE_HIP
	kaapi_driver_t* driver = kaapi_offload_driver_bytype( KAAPI_PROC_TYPE_HIP );
#endif
	driver->f_advise_cpu( ptr, size);
}

void xkblas_unregister_work_buffer( void* ptr )
{
	pthread_mutex_lock( &work_buffer_mutex );
	if( ((uintptr_t) ptr) != first_segment->ptr )
	{ // Well ...
	        printf("Try to unregister another ptr than registered one\n");
		exit(1);
	}

	struct memory_segment* curr = first_segment;
	while(curr)
	{
		struct memory_segment* next = curr->next;
		free(curr);
		curr = next;
	}
	free_segment_list = NULL;
	first_segment = NULL;
	total_size = 0;

	pthread_mutex_unlock( &work_buffer_mutex );
}


