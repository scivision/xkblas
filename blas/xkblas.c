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

#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "common.h"
#include "kaapi_impl.h"
#include "kaapi_offload.h"

#if KAAPI_USE_CUDA
#include <cuda_runtime_api.h>
#endif

#include "internal_register.h"

/* 2^KAAPI_SIZE_DSM_MAP is the size of the hash map */
#define KAAPI_SIZE_DSM_MAP 20

/* hash map: matrix -> xkblas_matrix_descr_t */
static kaapi_hashmap_t _kblas_ptr2handle;

/* use ->pos as index of the next handle to allocated in ->data */
static kaapi_stack_bloc_t* curr_blochandle = 0;
static kaapi_stack_bloc_t* freelist_blochandle = 0;
static kaapi_hashentries_t* _kblas_mapentries[1ULL<<KAAPI_SIZE_DSM_MAP];
static kaapi_team_t* _xkblas_team = 0;
kaapi_thread_t* _xkblas_thread =0;

__thread kaapi_thread_t* _xkblas_self_thread = 0;

kaapi_handle_t* _xkblas_list_sync0 = 0; /* re-used unused sync0->sync field as ptr to handle */
static xkblas_matrix_descr_t* _xkblas_matrix_descr_list = 0;
static uint64_t _xkblas_generation_cache = 0;

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
  return (Ah->addr != 0) && (Ah->gen == _xkblas_generation_cache);
}


/* Find the internal descriptor for A
*/
xkblas_matrix_descr_t* xkblas_find( const void* A )
{
  xkblas_matrix_descr_t** me;
  kaapi_hashentries_t* entry;
  //printf("xkblas_find:%i\n", ++cnt_xkblas_find);
  entry = kaapi_hashmap_findinsert( &_kblas_ptr2handle, A );
  me = KAAPI_HASHENTRIES_GETREF(entry, xkblas_matrix_descr_t*);
  if (*me == 0)
  {
    xkblas_matrix_descr_t* Ah = (xkblas_matrix_descr_t*)malloc(sizeof(struct xkblas_matrix_descr));
    /* marker for xkblas_matrix_descr_isinit return false */
    Ah->handle = 0;
    Ah->addr = 0;
    Ah->capacity = 0;
    Ah->next = _xkblas_matrix_descr_list;
    _xkblas_matrix_descr_list = Ah;
    *me = Ah;
  }
  return *me;
}


int xkblas_init_matrix_handle( xkblas_matrix_descr_t* Ah,
  void* A, size_t M, size_t N, size_t LD, size_t eltsize, size_t MB, size_t NB
)
{
  size_t default_tilesize = xkblas_get_param();

  Ah->addr = A;
  Ah->eltsize = eltsize;
  Ah->ld = LD;
  Ah->M  = M;
  Ah->N  = N;
  Ah->mb = MB;
  Ah->nb = NB;

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
      kaapi_handle_init(_xkblas_thread, handle, ADDR_(A,m,n));
#if 0//DEBUG
char* name =kaapi_dbg_get_name(Ah);
printf("New handle (%i,%i) / %s\n",m,n, name == 0 ? "" : name );
#endif
      handle->sync0.sync = (kaapi_access_t*)_xkblas_list_sync0;
      _xkblas_list_sync0 = handle;
      Ah->ldid[m*Ah->nt+n] = 0;
    }

  Ah->gen = _xkblas_generation_cache;
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

  if (lid != (uint16_t)0)
    return lid-1;

  unsigned int count = kaapi_localitydomain_count(KAAPI_LD_GPU);
  if (count ==0)
  {
    lid = lid0;
    goto retval;
  }

  kaapi_handle_t* h = xkblas_get_handle(Ah, i, j);
  kaapi_metadata_info_t* mdi = h->last->mdi;
  if (mdi ==0)
  {
    if (count >0)
      lid = kaapi_localitydomain_get_num(KAAPI_LD_GPU, rand()%count);
    else
      lid = lid0;
    goto retval;
  }
  else {
    lid = _kaapi_get_valid_lid( mdi, 0, 0 );
    if ((lid ==lid0) && (count >0))
      lid = kaapi_localitydomain_get_num(KAAPI_LD_GPU, rand()%count);
  }
retval:
  Ah->ldid[ i*Ah->nt + j ] = lid+1;
  return lid;
}


/* NB = tile size. 0 == value computed at runtime
*/
static size_t NB = 0;
void xkblas_set_param(size_t nb, size_t p)
{
  printf("In %s: nb:%li, p: %li\n", __func__, nb, p );
  NB=nb;
  if (p>= sizeof(double)) /* max precision */
    p = 16;
  kaapi_memory_set_info( KAAPI_MEMORY_EXPECTED_BLOCK, nb*nb* p );
}


/*
*/
size_t xkblas_get_param(void)
{
  return NB;
}


/*
*/
int xkblas_get_ngpus(void)
{
  return (int)kaapi_default_param.ngpus;
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
  kaapi_default_param.ngpus = ngpus;
  return err;
}

/*
 */
void* xkblas_malloc( size_t size )
{
#if KAAPI_USE_CUDA
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
#if KAAPI_USE_CUDA
  kaapi_assert_m(cudaSuccess== cudaFreeHost(ptr),"cudaFreeHost failed");
#else
  free(ptr);
#endif
}


/* Asynchronous operation, return handle to the request
 */
uint64_t xkblas_register_memory_async( void* ptr, size_t sz )
{

#if KAAPI_USE_CUDA
  kaapi_driver_t* driver = kaapi_offload_deriver_bytype( KAAPI_PROC_TYPE_CUDA );
  if (driver ==0) return 0;
  return driver->f_host_register( ptr, sz, 0, 0, 0, 0);
#endif
  return 0;
}

/* Test if the request is completed
*/
int xkblas_register_memory_test( uint64_t handle )
{
#if KAAPI_USE_CUDA
  kaapi_driver_t* driver = kaapi_offload_deriver_bytype( KAAPI_PROC_TYPE_CUDA );
  if (driver ==0) return 1; /* always completed */
  return driver->f_host_register_testwait( handle, 0 );
#endif
  return 0;
}


/* Return the error code of the memory registration request handle
*/
int xkblas_register_memory_wait( uint64_t handle )
{
#if KAAPI_USE_CUDA
  kaapi_driver_t* driver = kaapi_offload_deriver_bytype( KAAPI_PROC_TYPE_CUDA );
  if (driver ==0) return 1; /* always completed */
  return driver->f_host_register_testwait( handle, 1 );
#endif
  return 0;
}


/*
*/
int xkblas_register_memory_waitall( )
{
#if KAAPI_USE_CUDA
  kaapi_driver_t* driver = kaapi_offload_deriver_bytype( KAAPI_PROC_TYPE_CUDA );
  if (driver ==0) return 1; /* always completed */
  return driver->f_host_register_testwait( 0, 2 );
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
#if KAAPI_USE_CUDA
  kaapi_driver_t* driver = kaapi_offload_deriver_bytype( KAAPI_PROC_TYPE_CUDA );
  if (driver ==0) return 0;
  return driver->f_host_unregister( ptr, sz );
#endif
  return 0;
}

/*
*/
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


/* Do not implement type== ALL
   store bloc (i,j) on a grid of ressource GpxGq (i/Bp)%Gp,(j/Bq)%Gq
   If matrix is not found return EINVAL
*/
int xkblas_map_2Dblock_cyclic(
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
      if ((ldid ==0) || force)
      {
        int r = ( ((i/Bp)%Gp)*Gq + (j/Bq)%Gq ) %count;
        xkblas_set_ldid(Ah, i, j, ldid = 1+kaapi_localitydomain_get_num(type, r));
#if 0
        const char* name = kaapi_dbg_get_name(A);
        printf("SET: %s @: %p -> (%i,%i) = %i\n",
            (name ? name : ""),
            addr,
            ((i/Bp)%Gp),
            (j/Bq)%Gq,
            ldid-1
        );
#endif
      }
#if 0
      const char* name = kaapi_dbg_get_name(A);
      if (name)
        printf("%s=%i  ",
          (name ? name : ""),
          ldid-1
        );
      else
        printf("(%i,%i)=%i  ",
          i,j,
          ldid-1
        );
#endif
    }
#if 0
    printf("\n");
#endif
  }
  return 0;
}

/* Do not implement type== ALL
   store bloc [i,.] on a ressource (i/B)%G
   colrow = 0 -> col mapping
   colrow = 1 -> row mapping
*/
int xkblas_map_1Dblock_cyclic(
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
    return xkblas_map_2Dblock_cyclic(
      hlevel, storage, m, n, A, lda, eltsize,
      (m+MB-1)/MB, B,
      1, G,
      force
    );
  }
  else
  {
    return xkblas_map_2Dblock_cyclic(
      hlevel, storage, m, n, A, lda, eltsize,
      B, (n+NB-1)/NB,
      G, 1,
      force
    );
  }
  printf("[%s] invalid colrow argument\n");
#if KAAPI_DEBUG
  abort();
#endif
  return EINVAL;
}


/*
*/
#define STRNAME  "writeback"
#define NAME(x) kaapi_##x##_writeback
#define PNAME(x) kaapi_writeback_##x
#define NPARAM 1
#define MODE_PARAM {KAAPI_ACCESS_MODE_R}
#define ADDR_PARAM {&arg->a}
#define VIEW_PARAM {{ &arg->m, &arg->n, &arg->ld}}
#define FORMAT_TYPE kaapi_char_format
#define SIZEOF_TYPE arg->eltsize
#define DOT_COLOR "gray"
#define DOT_SHAPE "octagon"

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
  //printf("[%s] should never be call on CPU !!!\n", __func__);
  //abort();
}

/*
*/
#if KAAPI_USE_CUDA
#if KAAPI_DEBUG
kaapi_atomic_t pending_writeback={0};
kaapi_atomic_t received_writeback={0};
#endif
static void callback_epilogue_writeback(
    kaapi_io_status_t status,
    kaapi_io_stream_t* ios,
    void* arg0, void* arg1, void* arg2
)
{
  kaapi_metadata_info_t* mdi __attribute__((unused)) = (kaapi_metadata_info_t*)arg0;
  kaapi_frame_t* frame = (kaapi_frame_t*)arg1;
#if KAAPI_DEBUG
  KAAPI_ATOMIC_INCR(&received_writeback);
#endif
  KAAPI_ATOMIC_INCR(&frame->exec_count);
}

static void NAME(task_body_gpu)( kaapi_task_t* task, kaapi_thread_t* thread, void* handle )
{
  /* Make execution as if task_body_gpu spawn continuation (reception of communication)
     in order to detect that end of tasks execution
  */
  NAME(Arg)* taskarg = kaapi_task_getargst(task,NAME(Arg));
#if KAAPI_DEBUG
  KAAPI_ATOMIC_INCR(&pending_writeback);
#endif
  int err = kaapi_dsm_prefetch_on( &kaapi_the_dsm, kaapi_local_asid,
    taskarg->a.mdi,
    callback_epilogue_writeback, taskarg->a.mdi, taskarg->frame, taskarg
  );
  kaapi_assert((err==0) || (err== EINPROGRESS));
}
#endif

#include "task_format.h"


/* Create write back task for the tile A(m,n) of size compute in the function
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
  kaapi_ldid_t ldid = xkblas_get_ld(Ah, m, n);
  kaapi_taskflag_set(task, KAAPI_TASK_FLAG_INCOM);
  kaapi_task_set_ld(task, 0, ldid);
  /* incr the fact that spawned task should be considered completed when epilogue is called */
  KAAPI_ATOMIC_INCR(&taskarg->frame->spawn_count);
  kaapi_task_commit( thread, task );
}


/*
*/
#define STRNAME  "invalidate"
#define NAME(x) kaapi_##x##_invalidate
#define PNAME(x) kaapi_invalidate_##x
#define NPARAM 1
#define MODE_PARAM {KAAPI_ACCESS_MODE_RW}
#define ADDR_PARAM {&arg->a}
#define VIEW_PARAM {{ &arg->m, &arg->n, &arg->ld}}
#define FORMAT_TYPE kaapi_char_format
#define SIZEOF_TYPE arg->eltsize
#define DOT_COLOR "gray0"
#define DOT_SHAPE "invtriangle"

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
#if KAAPI_USE_CUDA
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
  kaapi_ldid_t ldid = xkblas_get_ld(Ah, m, n);
  kaapi_taskflag_set(task, KAAPI_TASK_FLAG_INCOM);
  kaapi_task_set_ld(task, 0, ldid);
  kaapi_task_commit( thread, task );
}




/*
*/
#define STRNAME  "distribute"
#define NAME(x) kaapi_##x##_distribute
#define PNAME(x) kaapi_distribute_##x
#define NPARAM 1
#define MODE_PARAM {KAAPI_ACCESS_MODE_RW}
#define ADDR_PARAM {&arg->a}
#define VIEW_PARAM {{ &arg->m, &arg->n, &arg->ld}}
#define FORMAT_TYPE kaapi_char_format
#define SIZEOF_TYPE arg->eltsize
#define DOT_COLOR "gray1"
#define DOT_SHAPE "triangle"

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
#if KAAPI_USE_CUDA
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
  kaapi_task_set_ld(task, 0, ldid);
  kaapi_task_commit( thread, task );
}




/*
*/
static int init_count = 0;
int xkblas_init(void)
{
  if (init_count++ !=0) return 0;

//printf("Xblas_init, hashmap:%p\n",&_kblas_ptr2handle);
  int err = kaapi_hashmap_init(&_kblas_ptr2handle, _kblas_mapentries, KAAPI_SIZE_DSM_MAP, 0);
  kaapi_assert(err ==0);

  if (getenv("XKBLAS_TILE_SIZE") || getenv("XKBLAS_PRECISION"))
  {
    size_t tile_size = 0;
    if (getenv("XKBLAS_TILE_SIZE") ==0)
      tile_size = xkblas_get_param();
    else
      tile_size = atoi(getenv("XKBLAS_TILE_SIZE"));

    size_t precision = sizeof(double);
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
        fprintf(stderr,"[XKBLAS] XKBLAS_PRECISION must be = float|real*4|double|read*8|complex|complex32|complex*4|complex64|complex*8.\n");
        exit(1); 
      }
    }
    if (getenv("KAAPI_VERBOSE"))
    {
      printf("[XKBLAS] tile size: %lu, precision: %lu\n",tile_size, precision);
    }
    xkblas_set_param( tile_size, precision );
  }

  if (getenv("XKBLAS_NGPUS"))
    setenv("KAAPI_NUM_GPUS",getenv("XKBLAS_NGPUS"),1);

  if (getenv("XKBLAS_GPUSET"))
    setenv("KAAPI_GPUSET",getenv("XKBLAS_GPUSET"),1);

  if (getenv("XKBLAS_NCUDA_STREAMS"))
    setenv("KAAPI_CUDA_KERNEL_STREAM_NUMS",getenv("XKBLAS_NCUDA_STREAMS"),1);

  if (getenv("XKBLAS_NKERNELS_PER_STREAM"))
    setenv("KAAPI_CUDA_KERNEL_PER_STREAM",getenv("XKBLAS_NKERNELS_PER_STREAM"),1);

  if (getenv("XKBLAS_CACHE_LIMIT"))
    setenv("KAAPI_CUDA_CACHE_LIMIT",getenv("XKBLAS_CACHE_LIMIT"),1);

  xkblas_register_task_format();
  kaapi_register_format_writeback();
  kaapi_register_format_invalidate();
  kaapi_register_format_distribute();
  kaapi_init();

  extern const char* get_kaapi_version(void);
  printf("[XKBLAS init] %s\n", get_kaapi_version() );

  if (getenv("KAAPI_VERBOSE"))
  {
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
      printf("  lid[%i]=%i,  %s\n", (int)i, (int)kaapi_localitydomain_get_num(KAAPI_LD_GPU, i), kaapi_localitydomain_info(KAAPI_LD_GPU, i) );
    }
  }

  /* */
  _xkblas_team = kaapi_team_alloc();
  _xkblas_self_thread =  _xkblas_thread = kaapi_thread_bind(KAAPI_PROC_TYPE_HOST);
  kaapi_team_attach(_xkblas_team, _xkblas_thread, 1);

  kaapi_begin_dfg( _xkblas_thread, KAAPI_FRAME_FLAG_DFG_OK );
}


/*
*/
int xkblas_finalize(void)
{
  if (--init_count !=0) return 0;

  int err;

  kaapi_team_barrier_wait(_xkblas_team, _xkblas_thread, 1, KAAPI_BARRIER_FLAG_DEFAULT);
  kaapi_end_dfg( _xkblas_thread );
  kaapi_memory_synchronize();

  kaapi_team_deattach(_xkblas_team, _xkblas_thread);

  kaapi_context_t* ctxt = kaapi_thread2context(_xkblas_thread);
  xkblas_free_curr_blochandle();
  while (freelist_blochandle !=0)
  {
    kaapi_stack_bloc_t* bloc = freelist_blochandle;
    freelist_blochandle = freelist_blochandle->next;
    kaapi_stackallocator_dealloc(&ctxt->st_allocator, bloc );
  }

  kaapi_thread_unbind(_xkblas_thread);
  _xkblas_self_thread = _xkblas_thread = 0;
  kaapi_team_dealloc(_xkblas_team);

  err = kaapi_hashmap_clear(&_kblas_ptr2handle);
  kaapi_assert(err ==0);
  err = kaapi_hashmap_destroy(&_kblas_ptr2handle);
  kaapi_assert(err ==0);

  kaapi_finalize();
}


/*
*/
int xkblas_sync(void)
{
//printf("------------ xkblas_sync: %i\n",_xkblas_generation_cache);
//#undef KAAPI_DEBUG
//#define KAAPI_DEBUG 1
#if KAAPI_DEBUG
  size_t cnt_activated_handle = 0;
  size_t count = 0;
#endif
  /* activate first synchronisation access */
  kaapi_handle_t* curr = _xkblas_list_sync0;
  while (curr != 0)
  {
    kaapi_handle_t* next = (kaapi_handle_t*)curr->sync0.sync;
    if (curr->sync != 0)
    {
#if KAAPI_DEBUG
      count += 
#endif
        kaapi_sched_activate_syncpoint(_xkblas_thread, 0, &curr->sync0, 0);
#if KAAPI_DEBUG
      ++cnt_activated_handle;
#endif
    }
    curr = next;
  }

//#undef KAAPI_DEBUG
//#define KAAPI_DEBUG 0
//printf("------------ xkblas_sync: %i -> #handle: %i, #count activated: %i\n",_xkblas_generation_cache, cnt_activated_handle, count);

  kaapi_sched_sync(_xkblas_thread);

  kaapi_end_dfg( _xkblas_thread );
#define OLD_FLUSH 0
#if OLD_FLUSH // see xkblas_memory_invalidate_caches below
  xkblas_free_curr_blochandle();
  kaapi_hashmap_clear(&_kblas_ptr2handle);
  _xkblas_list_sync0 = 0;
#else
  /* reset all entries in _xkblas_list_sync0 */
  curr = _xkblas_list_sync0;
  _xkblas_list_sync0 = 0;
  while (curr != 0)
  {
    kaapi_handle_t* next = (kaapi_handle_t*)curr->sync0.sync;
    kaapi_handle_init(_xkblas_thread, curr, curr->sync0.data);
    curr->sync0.sync = (kaapi_access_t*)_xkblas_list_sync0;
    _xkblas_list_sync0 = curr;
    curr = next;
  }
#endif

  kaapi_begin_dfg( _xkblas_thread, KAAPI_FRAME_FLAG_DFG_OK );
}


int xkblas_memory_invalidate(const void* A)
{
  // TODO
  return 0;
}


/*
 */
int xkblas_memory_invalidate_caches(void)
{
  kaapi_memory_invalidate_caches();
#if OLD_FLUSH //see just above
  xkblas_free_curr_blochandle();
  kaapi_hashmap_clear(&_kblas_ptr2handle);
  _xkblas_list_sync0 = 0;
#else
  ++_xkblas_generation_cache;
  _xkblas_list_sync0 = 0;
#endif
}


/*
 */
int xkblas_memory_free(void)
{
  kaapi_memory_invalidate_caches();
  /* free matrix handle list */
  while (_xkblas_matrix_descr_list !=0)
  {
    xkblas_matrix_descr_t* next = _xkblas_matrix_descr_list->next;
    free(_xkblas_matrix_descr_list->handle);
    free(_xkblas_matrix_descr_list);
    _xkblas_matrix_descr_list = next;
  }
  /* kaapi_handle_t are store in matrix descriptor */
  _xkblas_list_sync0 = 0;
  kaapi_hashmap_clear(&_kblas_ptr2handle);

#if OLD_FLUSH //see just above
  xkblas_free_curr_blochandle();
  kaapi_hashmap_clear(&_kblas_ptr2handle);
  _xkblas_list_sync0 = 0;
#endif
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
}

/*
*/

int xkblas_memory_coherent_async(
  int uplo, int memflag,
  size_t M, size_t N,
  void* A, size_t LD, size_t eltsize
)
{
#if 0
  printf("-----------------------------\n");
  for (size_t i=0; i<kaapi_localitydomain_count(KAAPI_LD_GPU); ++i)
  {
    printf("  lid[%i]=%i,  %s\n", (int)i, (int)kaapi_localitydomain_get_num(KAAPI_LD_GPU, i), kaapi_localitydomain_info(KAAPI_LD_GPU, i) );
    kaapi_localitydomain_t* ld = kaapi_localitydomain_get( kaapi_localitydomain_get_num(KAAPI_LD_GPU, i) );
    kaapi_memory_cache_print( kaapi_memory_device_get( ld->device->memdev.asid ) );
  }
#endif

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

#if 0
  xkblas_sync();
  printf("-----------------------------\n");
  for (size_t i=0; i<kaapi_localitydomain_count(KAAPI_LD_GPU); ++i)
  {
    printf("  lid[%i]=%i,  %s\n", (int)i, (int)kaapi_localitydomain_get_num(KAAPI_LD_GPU, i), kaapi_localitydomain_info(KAAPI_LD_GPU, i) );
    kaapi_localitydomain_t* ld = kaapi_localitydomain_get( kaapi_localitydomain_get_num(KAAPI_LD_GPU, i) );
    kaapi_memory_cache_print( kaapi_memory_device_get( ld->device->memdev.asid ) );
  }
#endif

  return 0;
}


/* 2D data distribute
   store bloc (i,j) on a grid of ressource GpxGq (i/Bp)%Gp,(j/Bq)%Gq
   If matrix is not found return EINVAL
*/
int xkblas_distribute_2Dblock_cyclic_async(
  int hlevel, int storage, size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize,
  size_t Bp, size_t Bq, /* blocking size */
  size_t Gp, size_t Gq  /* grid size */
)
{
  xkblas_matrix_descr_t* Ah = xkblas_find(A);
  if (!xkblas_matrix_descr_isinit(Ah)) 
  {
    size_t NB = xkblas_get_param();
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
      uint16_t ldid = kaapi_localitydomain_get_num(type, r);
      xkblas_create_distribute( Ah, i, j, lda, eltsize, ldid );
      xkblas_set_ldid(Ah, i, j, 1+ldid );
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
  int hlevel, int storage, int colrow, size_t m, size_t n,
  const void* A, size_t lda, size_t eltsize,
  size_t B, size_t G    /* grid size */
)
{
  size_t MB,NB;
  MB = NB = xkblas_get_param();
  if (colrow == 0)
  {
    return xkblas_distribute_2Dblock_cyclic_async(
      hlevel, storage, m, n, A, lda, eltsize,
      (m+MB-1)/MB, B,
      1, G
    );
  }
  else
  {
    return xkblas_distribute_2Dblock_cyclic_async(
      hlevel, storage, m, n, A, lda, eltsize,
      B, (n+NB-1)/NB,
      G, 1
    );
  }
  return EINVAL;
}


/* Return the bloc size:
   - M,N should be the dimension of the result. K depends of the Kernel.
   Return NB such that (M,N) is computed in FACTOR*NGPU blocs
*/
size_t xkblas_auto_nb(
  xkblas_kernel_t kernel, size_t M, size_t N, size_t K
)
{
  switch (kernel)
  {
    case KERN_SYRK:
    case KERN_SYR2K:
    case KERN_HEMM:
    case KERN_HERK:
    case KERN_HER2K:
    case KERN_TRSM:
    case KERN_GEMMT:
    case KERN_GEMM:
    case KERN_TRMM:
    case KERN_SYMM:
    {
      /* get default tile size and initialize internal descriptor if not yet */
      size_t NB = xkblas_get_param();
      if (NB!=0) return NB;

    #define FACTOR 2
      size_t ngpu = xkblas_get_ngpus();
      double tNB = ((double)M*(double)N) / (double)(ngpu * FACTOR);
      tNB =  sqrt(tNB);
      NB = (size_t)tNB;
      if (NB ==0)
      {
        NB = N / ngpu;
        if (NB ==0) NB = 512;
      }
      //TG: TEST BEFORE making it permanent
      if (NB <128) NB = 128;
      return NB;
    };

    case KERN_VOID:
      break;
  }
  return 0;
}


/* Map tile of matrix descriptor.
   Hardcoded selection between 1D or 2D mapping depending of the type of kernel
*/
int xkblas_auto_map(
  xkblas_kernel_t kernel,
  xkblas_matrix_descr_t* Ah
)
{
  switch (kernel)
  {
    case KERN_SYRK:
    case KERN_SYR2K:
    case KERN_HEMM:
    case KERN_HERK:
    case KERN_HER2K:
/*
      break;
*/

    case KERN_TRSM:
/*
      xkblas_map_1Dblock_cyclic(
        1, CblasColMajor, 0,
        Ah->M, Ah->N, Ah->addr, Ah->ld, Ah->eltsize,
        1, xkblas_get_ngpus(), 0
      );
      break;
*/

    case KERN_GEMMT:
    case KERN_GEMM:
    case KERN_TRMM:
    case KERN_SYMM:
    { /* 2D Bloc cyclic */
      size_t Blkm = 1;
      size_t Blkn = 1;
      size_t ngpu = xkblas_get_ngpus();
      size_t Gm = sqrt(ngpu);
      size_t Gn = Gm;
      if (Gm ==0) { Gn = ngpu; Gm = 1; }
      else {
        /* find the most square decomposition of ngpu in Gm x Gn */
        size_t g;
        for (g = Gm+1; g>0; --g)
        {
           if (ngpu / g * g == ngpu) break;
        }
        if ((g==0) || (g*g != ngpu)) { Gm = ngpu; Gn = 1; }
      }
      //printf("Block2D cyclic: Blkm: %i, Blkn: %i, Gm: %i, Gn: %i\n", Blkm, Blkn, Gm, Gn);
      xkblas_map_2Dblock_cyclic(
        1, CblasColMajor,
        Ah->M, Ah->N, Ah->addr, Ah->ld, Ah->eltsize,
        Blkm, Blkn, Gm, Gn, 0
      );
    } break;

    case KERN_VOID:
      break;
  }
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

