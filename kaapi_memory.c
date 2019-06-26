/*
** Copyright 2009-2013,2018,2019 INRIA
**
** Contributors :
**
** thierry.gautier@inrialpes.fr
** joao.lima@inf.ufsm.br
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "kaapi_impl.h"
#include "kaapi_offload.h"
#include "kaapi_offload_stream.h"

/* 2^KAAPI_SIZE_DSM_MAP is the size of the hash map */
#define KAAPI_SIZE_DSM_MAP 20

/* DSM node representation
*/
struct kaapi_dsm_node {
  kaapi_hashentries_t*   mapentries[1ULL<<KAAPI_SIZE_DSM_MAP];
  kaapi_hashmap_t        ht;
  kaapi_lock_t           lock;
  kaapi_memory_device_t* device;
  kaapi_memory_cache_t*  cache;
};


/*
*/
kaapi_dsm_t kaapi_the_dsm;
kaapi_atomic32_t kaapi_dsm_asid_lid = {0}; /* 0 is the host memory */

/* fwd decl */
static int _kaapi_dsm_deallocate_replica(
    kaapi_dsm_t* dsm,
    kaapi_metadata_info_t* mdi
);

/* short cut */
kaapi_memory_device_t* kaapi_memory_device_get(
    kaapi_address_space_id_t asid
)
{
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  if (kaapi_the_dsm.nodes[lid] ==0) return 0;
  return kaapi_the_dsm.nodes[lid]->device;
}


#if KAAPI_DEBUG
static void _kaapi_memory_cache_check( kaapi_memory_cache_t* cache);
#endif

/* -------------------------------------------------------------------------- */
static inline bool kaapi_memory_replica_is_allocated(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  return  ((KAAPI_ATOMIC_READ(&mdi->alloc) & (1UL<<lid)) !=0);
}

static inline void kaapi_memory_replica_set_allocated(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  KAAPI_ATOMIC_OR64(&mdi->alloc, (1UL<<lid));
}

static inline void kaapi_memory_replica_unset_allocated(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  KAAPI_ATOMIC_AND64(&mdi->alloc, ~(uint64_t)(1UL<<lid));
}


static inline bool kaapi_memory_replica_is_valid_excepton(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  return  ((KAAPI_ATOMIC_READ(&mdi->valid) & ~(uint64_t)(1UL<<lid)) !=0);
}

static inline bool kaapi_memory_replica_is_valid_somewhere(
    kaapi_metadata_info_t*   mdi
)
{
  kaapi_assert_debug(mdi != 0);
  return  (KAAPI_ATOMIC_READ(&mdi->valid) !=0);
}

static inline void kaapi_memory_replica_set_valid(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  KAAPI_ATOMIC_OR64(&mdi->valid, (1UL<<lid));
}

static inline void kaapi_memory_replica_unset_valid(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  KAAPI_ATOMIC_AND64(&mdi->valid, ~(uint64_t)(1UL<<lid));
}

static inline void kaapi_memory_replica_set_all_dirty_except(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  KAAPI_ATOMIC_WRITE(&mdi->valid, (1UL<<lid));
}


static inline void kaapi_memory_replica_set_xfer(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  KAAPI_ATOMIC_OR64(&mdi->xfer, (1UL<<lid));
}

static inline void kaapi_memory_replica_unset_xfer(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  KAAPI_ATOMIC_AND64(&mdi->xfer, ~(uint64_t)(1UL<<lid));
}

static inline int kaapi_memory_replica_is_xfer_tosomewhere(
    kaapi_metadata_info_t*   mdi
)
{
  kaapi_assert_debug(mdi != 0);
  return KAAPI_ATOMIC_READ(&mdi->xfer) != 0;
}

static inline bool kaapi_memory_replica_is_notpinned(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  return  (mdi->replicas[lid] ==0) || (KAAPI_ATOMIC_READ(&mdi->replicas[lid]->pinned) ==0);
}

static inline void kaapi_memory_replica_mark_pinned(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
#if KAAPI_DEBUG
  int val = 
#endif
    KAAPI_ATOMIC_INCR(&mdi->replicas[lid]->pinned);
  kaapi_assert_debug( val <= 127 );
}

static inline void kaapi_memory_replica_unmark_pinned(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
#if KAAPI_DEBUG
  int value=  
#endif
    KAAPI_ATOMIC_DECR(&mdi->replicas[lid]->pinned);
#if KAAPI_DEBUG
   kaapi_assert( (lid!=0) || (value >0) );
#endif
}

static inline void kaapi_memory_replica_unset_pinned(
    kaapi_metadata_info_t*   mdi,
    uint16_t lid
)
{
  kaapi_assert_debug(mdi != 0);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  KAAPI_ATOMIC_WRITE(&mdi->replicas[lid]->pinned, 0);
}

/* -------------------------------------------------------------------------- */
static void bit2str( char* buffer, int bitmap )
{
  buffer[0] = 0;
  for (int i=7; i>=0; --i) 
  {
    if ( bitmap & (1<<i))
      buffer[7-i] = '1';
    else
      buffer[7-i] = '0';
  }
  buffer[8]=0;
}

/* -------------------------------------------------------------------------- */
struct kaapi_alloc_data {
  kaapi_pointer_t     ptr;
  size_t              size;
  kaapi_alloc_data_t* next;
};

/* If free list used for Tile matrix (blocsize always the same size)
   performance is good for the warmup run but poor for next runs
*/
#define MEM_ALLOC_FREELIST 1

#if MEM_ALLOC_FREELIST
static size_t DEFAULT_TILE_SIZE = 1*2048*2048*sizeof(double);
static size_t TILE_SIZE = 1*2048*2048*sizeof(double);
#endif
int kaapi_memory_set_info( int kind, size_t value )
{
  if (kind == KAAPI_MEMORY_VOID)
    value;
#if MEM_ALLOC_FREELIST
  else if (kind == KAAPI_MEMORY_EXPECTED_BLOCK)
  {
    TILE_SIZE = value;
    if (getenv("KAAPI_VERBOSE")) 
      printf("[xkaapi] preferred block size to %zu\n", value );
  }
#endif
  else
    return ENOENT;
  return 0;
}


#if KAAPI_DEBUG_MEMORY_ALLOC
#define KAAPI_SIZE_DBG_MAP_ALLOC 16
kaapi_hashmap_t      free_ptr_ht; /* to store task & data visited */
kaapi_hashentries_t* free_mapentries[1<<KAAPI_SIZE_DBG_MAP_ALLOC];
#endif

/* Return a device bloc of size at least size
*/
kaapi_pointer_t kaapi_memory_alloc(kaapi_address_space_id_t asid, size_t size)
{
  kaapi_memory_device_t* device = kaapi_memory_device_get(asid);
  kaapi_pointer_t ptr;
  kaapi_assert(device !=0);
#if MEM_ALLOC_FREELIST
  if (size <=TILE_SIZE)
  {
    size = TILE_SIZE;
    kaapi_alloc_data_t* kad = device->freelist_bloc;
    if (kad ==0)
    {
      ptr = kaapi_make_pointer((void*)device->f_alloc(device,TILE_SIZE), asid);
      //kaapi_assert_debug( !kaapi_pointer_isnull(ptr) );
      if (!kaapi_pointer_isnull(ptr))
        device->size_dev_alloc += TILE_SIZE;
//printf("New block: %li\n",size);
      return ptr;
    }
#if KAAPI_DEBUG_MEMORY_ALLOC
    kaapi_hashentries_t* entry = kaapi_hashmap_findinsert(&free_ptr_ht, (void*)kad->ptr.ptr);
    void* ref = KAAPI_HASHENTRIES_GET(entry, void*);
    if (ref ==0)
    {
       printf("*** Reuse freeed pointer not referenced in map???\n");
    }
    KAAPI_HASHENTRIES_SET(entry, 0, void*);
#endif
    ptr = kad->ptr;
    kaapi_assert_debug( !kaapi_pointer_isnull(ptr) );
    kaapi_assert_debug( size <= kad->size );
    kaapi_assert_debug( asid == kad->ptr.asid );
    device->freelist_bloc = kad->next;
    kad->next = device->freelist_metabloc;
    device->freelist_metabloc = kad;
    device->size_alloc += TILE_SIZE;
    return ptr;
  }
  else
#endif
  {
    ptr = kaapi_make_pointer((void*)device->f_alloc(device,size), asid);
    if (!kaapi_pointer_isnull(ptr))
      device->size_dev_alloc += size;
    kaapi_assert_debug( !kaapi_pointer_isnull(ptr) );
    return ptr;
  }
}


/*
*/
void kaapi_memory_free(kaapi_pointer_t ptr, size_t size )
{
  if (kaapi_pointer_isnull(ptr)) return;

#if KAAPI_DEBUG_MEMORY_ALLOC
  kaapi_hashentries_t* entry = kaapi_hashmap_findinsert(&free_ptr_ht, (void*)ptr.ptr);
  void* ref = KAAPI_HASHENTRIES_GET(entry, void*);
  if (ref !=0)
  {
    printf("Already freed pointer !\n"); 
  }
  KAAPI_HASHENTRIES_SET(entry, (void*)ptr.ptr, void*);
#endif

  kaapi_memory_device_t* device = kaapi_memory_device_get(ptr.asid);
  kaapi_assert(device !=0);
#if MEM_ALLOC_FREELIST
  if (size <=TILE_SIZE)
  {
#define KAD_BLOCSIZE 64
    kaapi_alloc_data_t* kad = device->freelist_metabloc;
    if (kad ==0)
    {
      kad = malloc(sizeof(kaapi_alloc_data_t)*KAD_BLOCSIZE);
      for (int i=0; i<KAD_BLOCSIZE; ++i)
      {
        kad[i].ptr = kaapi_make_nullpointer(ptr.asid);
        kad[i].next = &kad[i+1];
        kad[i].size = 0;
      }
      kad[KAD_BLOCSIZE-1].next = 0;
      device->freelist_metabloc = &kad[1];
    }
    device->freelist_metabloc = kad->next;
    kad->size = TILE_SIZE;
    kad->ptr  = ptr;
    kad->next = device->freelist_bloc;
    device->freelist_bloc = kad;
    device->size_free += TILE_SIZE;
    kaapi_assert_debug( device->size_free <= device->size_alloc+device->size_dev_alloc );
  }
  else
#endif
  {
    device->f_free(device,ptr.ptr, size);
    device->size_dev_free += size;
  }
}

/* -------------------------------------------------------------------------- */
/* cache data structure for dsm
   here is an implementation of our IPDPS2013 paper cache with separate entry for ro an
   data that are writen.
   Each replica is managed into the cache of its corresponding memory device.
   replica has both cacheentry and cachelist where cachelist points to the ro or rw list
   and cache entry points to the entry in the list.
*/
#define KAAPI_CACHEENTRY_BLOCSIZE 1024
typedef struct kaapi_cache_entry {
  kaapi_metadata_info_t* mdi;
  struct kaapi_cache_entry *next;
  struct kaapi_cache_entry *prev;
} kaapi_cache_entry_t;

/* entry[0] is reserved for next and pos */
typedef struct kaapi_cache_blocentry {
  union {
    struct {
      struct kaapi_cache_blocentry* next;
      int pos;
    };
    kaapi_cache_entry_t entry[KAAPI_CACHEENTRY_BLOCSIZE];
  };
} kaapi_cache_blocentry_t;

typedef struct {
    kaapi_cache_entry_t *beg;
    kaapi_cache_entry_t *end;
} kaapi_cache_list_t;

typedef struct kaapi_memory_cache {
  kaapi_address_space_id_t asid;
  size_t size_limit;
  size_t size_used;                /* sum of data in ro and rw */
  kaapi_cache_list_t ro;
  kaapi_cache_list_t rw;
  kaapi_cache_entry_t* freelist;
  kaapi_cache_blocentry_t* allocated_bloc;
} kaapi_cache_lru_double_fifo_t;


/** \ingroup Offload
 * Allocates an empty software cache for the device.
 * All blocks are in a FIFO double-ended queue, LRU policy.
 */
static kaapi_memory_cache_t* kaapi_memory_cache_init(
  kaapi_memory_device_t* device,
  kaapi_address_space_id_t asid,
  int flags
)
{
  kaapi_memory_cache_t* cache = malloc(sizeof(kaapi_memory_cache_t));
  cache->asid  = asid;
  cache->size_limit = kaapi_default_param.cuda_cache_limit*device->f_get_free_mem(device);
  cache->size_used  = 0;
  cache->ro.beg = cache->ro.end = 0;
  cache->rw.beg = cache->rw.end = 0;
  cache->freelist = 0;
  cache->allocated_bloc = 0;
  return cache;
}

/** \ingroup Offload
 */
static inline int kaapi_memory_cache_destroy(kaapi_memory_cache_t* cache)
{
  if (cache ==0) return 0;
  while (cache->allocated_bloc !=0)
  {
    kaapi_cache_blocentry_t* bloc = cache->allocated_bloc;
    cache->allocated_bloc = bloc->next;
    free(bloc);
  }
  free(cache);
  return 0;
}


/* New entry for a cache list
*/
static kaapi_cache_entry_t* kaapi_memory_cache_allocate_entry(kaapi_memory_cache_t* cache)
{
  kaapi_cache_entry_t* entry = cache->freelist;
  if (entry ==0)
  {
    kaapi_cache_blocentry_t* bloc = cache->allocated_bloc;
    if ((bloc ==0) || (bloc->pos == KAAPI_CACHEENTRY_BLOCSIZE))
    {
      bloc = (kaapi_cache_blocentry_t*)malloc(sizeof(kaapi_cache_blocentry_t));
      bloc->next = cache->allocated_bloc;
      cache->allocated_bloc = bloc;
      bloc->pos = 1;
    }
    entry = &bloc->entry[bloc->pos++];
  } else
    cache->freelist = entry->next;
  return entry;
}


/* Remove entry from the list
*/
static void kaapi_memory_cache_remove_from_list( kaapi_cache_list_t* list, kaapi_cache_entry_t* entry)
{

  if (entry->next !=0) entry->next->prev = entry->prev;
  else list->end = entry->prev;
  if (entry->prev !=0) entry->prev->next = entry->next;
  else list->beg = entry->next;
  entry->prev = entry->next = 0;
}


/* Push entry on front the list
*/
static void kaapi_memory_cache_push_front( kaapi_cache_list_t* list, kaapi_cache_entry_t* entry)
{
  entry->next = list->beg;
  entry->prev = 0;
  if (list->beg ==0)
    list->end = entry;
  else
    list->beg->prev = entry;
  list->beg = entry;
}


/* Touch data: move cache entry to hot entries list
*/
static int kaapi_memory_cache_touch(
  kaapi_dsm_t*           dsm,
  uint16_t               lid,
  kaapi_access_mode_t    mode,  /* ro, rw, w, ... */
  kaapi_metadata_info_t* mdi
)
{
  if (lid ==0) return 0; /* host memory: no cache */

  kaapi_data_replica_t* data = mdi->replicas[lid];
  kaapi_memory_cache_t* cache = dsm->nodes[lid]->cache;
  int tid = dsm->nodes[lid]->device->device->ctxt->tid;
  kaapi_cache_list_t* list;
  if (KAAPI_ACCESS_IS_WRITE(mode))
    list = &cache->rw;
  else
    list = &cache->ro;

#if KAAPI_DEBUG
  _kaapi_memory_cache_check( cache );
#endif

  /* previous entry ? */
  kaapi_cache_list_t* oldlist = (kaapi_cache_list_t*)data->cachelist;
  kaapi_cache_entry_t* entry = (kaapi_cache_entry_t*)data->cacheentry;
  if (entry ==0) /* not in this cache */
  {
    entry = kaapi_memory_cache_allocate_entry(cache);
    entry->mdi  = mdi;
    data->cacheentry = entry;
    cache->size_used += kaapi_memory_view_size(&data->view);

    ++thread_stat[tid].counter[KAAPI_CNT_CACHE_MISS];
    thread_stat[tid].counter[KAAPI_CNT_CACHE_MISS_BYTES] +=
      kaapi_memory_view_size( &data->view );
  }
  else 
  { /* to not change the accounting of size_used,
       because oldlist is either the cache's ro or rw
    */
    kaapi_assert_debug((oldlist==&cache->rw)||(oldlist==&cache->ro));
    kaapi_memory_cache_remove_from_list( oldlist, entry );

    ++thread_stat[tid].counter[KAAPI_CNT_CACHE_HIT];
    thread_stat[tid].counter[KAAPI_CNT_CACHE_HIT_BYTES] +=
      kaapi_memory_view_size( &data->view );
  }
  /* if item previously in rw list, then keep it on rw list */
  if (oldlist == &cache->rw)
    list = &cache->rw;
  kaapi_assert_debug((list==&cache->rw)||(list==&cache->ro));
  kaapi_memory_cache_push_front( list, entry );
  data->cachelist = list;

#if KAAPI_DEBUG
  _kaapi_memory_cache_check( cache );
#endif
  return 0;
}


/*
 * */
static size_t _kaapi_memory_cache_sizelist( kaapi_memory_cache_t* cache, kaapi_cache_list_t* list)
{
  uint16_t lid = kaapi_memory_asid_get_lid(cache->asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  kaapi_cache_entry_t* curr = list->beg;
  size_t size = 0;
  while (curr !=0)
  {
    size += kaapi_memory_view_size(&curr->mdi->replicas[lid]->view);
    curr = curr->next;
  }
  return size;
}



/*
*/
static void _kaapi_memory_cache_print( kaapi_memory_cache_t* cache)
{
  uint16_t lidhost = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lidhost < KAAPI_MEMORY_MAX_NODES);
  uint16_t lid = kaapi_memory_asid_get_lid(cache->asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  char valid[16];
  char alloc[16];
  char pin[16];
  char xfer[16];

  printf("%2i:: cache asid:%i  info: size_limit:%lu, size_used:%lu\n", (int)lid, (int)lid, cache->size_limit, cache->size_used);
  for (int it = 0; it < 2; ++it)
  {
    kaapi_cache_list_t* list;
    if (it ==0) list = &cache->ro;
    else list = &cache->rw;


    /* compute size of list before dumping each entry */
    size_t size = _kaapi_memory_cache_sizelist(cache, list);
    printf("%2i:: list %s: @:%p, size_cached:%lu\n", lid, (list  == &cache->ro ? "RO" : "RW"), (void*)list, size);

    kaapi_cache_entry_t* curr = list->beg;
    int count = 0;
    while (curr != 0)
    {
      bit2str( valid, KAAPI_ATOMIC_READ(&curr->mdi->valid) );
      bit2str( alloc, KAAPI_ATOMIC_READ(&curr->mdi->alloc) );
      int pinned = 0;
      for (int k=0;k<KAAPI_MEMORY_MAX_NODES; ++k)
        if ((curr->mdi->replicas[k] !=0) && (KAAPI_ATOMIC_READ(&curr->mdi->replicas[k]->pinned)>0))
          pinned |= (1<<k);
      bit2str( pin, pinned );
      bit2str( xfer, KAAPI_ATOMIC_READ(&curr->mdi->xfer) );
#if 1
      const char* name = kaapi_dbg_get_name((void*)curr->mdi->replicas[lidhost]->ptr.ptr);
#endif
      printf("%2i:: %i  curr: %p, %s data: %p/ (0)%p, valid: %s, alloc: %s, pin: %s, xfer: %s, size: %lu\n",
         lid,
         count++,
         (void*)curr,
         (name == 0 ?  "" : name),
         (void*)curr->mdi->replicas[lid]->ptr.ptr,
         (void*)curr->mdi->replicas[lidhost]->ptr.ptr,
         valid, alloc, pin, xfer,
         kaapi_memory_view_size(&curr->mdi->replicas[lid]->view)
      );
      kaapi_assert(kaapi_memory_view_size(&curr->mdi->replicas[lid]->view) ==
         kaapi_memory_view_size(&curr->mdi->replicas[lidhost]->view)
      );
      curr = curr->next;
    }
  }
}

void kaapi_memory_cache_print( kaapi_memory_device_t* device)
{
  _kaapi_memory_cache_print( kaapi_the_dsm.nodes[kaapi_memory_asid_get_lid(device->asid)]->cache );
}



#if KAAPI_DEBUG==1
/*
 */
static void _kaapi_memory_cache_check( kaapi_memory_cache_t* cache)
{
  uint16_t lidhost = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lidhost < KAAPI_MEMORY_MAX_NODES);
  uint16_t lid = kaapi_memory_asid_get_lid(cache->asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  int flagerr = 0;
  size_t size_ro = _kaapi_memory_cache_sizelist(cache, &cache->ro);
  size_t size_rw = _kaapi_memory_cache_sizelist(cache, &cache->rw);
  if (size_ro + size_rw != cache->size_used)
  {
    fprintf(stderr,"*** cache corrupted, sizes does not correspond\n");
    flagerr = 1;
    kaapi_assert( flagerr == 0);
  }

  for (int it = 0; it < 2; ++it)
  {
    kaapi_cache_list_t* list;
    if (it ==0) list = &cache->ro;
    else list = &cache->rw;

    kaapi_cache_entry_t* curr = list->beg;
    while (curr != 0)
    {
      if (kaapi_memory_view_size(&curr->mdi->replicas[lid]->view) !=
          kaapi_memory_view_size(&curr->mdi->replicas[lidhost]->view))
      {
        abort();
      }
      if (curr->mdi->replicas[lid]->ptr.ptr ==0)
      {
        fprintf(stderr,"*** cache corrupted, null pointer cached\n");
        flagerr = 1;
      }
      if (curr->mdi->replicas[lid]->cacheentry != curr)
      {
        fprintf(stderr,"*** cache corrupted, cacheentry differ\n");
        flagerr = 1;
      }
      if (curr->mdi->replicas[lid]->cachelist != list)
      {
        fprintf(stderr,"*** cache corrupted, cachelist differ\n");
        flagerr = 1;
      }
      curr = curr->next;
    }
  }
  if (flagerr)
    _kaapi_memory_cache_print(cache);

  kaapi_assert( flagerr == 0);
}
#endif



/* evict at least size bytes of object in the cache
*/
static size_t kaapi_memory_cache_evict_fromlist(
  kaapi_memory_device_t* device,
  kaapi_memory_cache_t* cache,
  size_t size,
  kaapi_cache_list_t* list
)
{
  uint16_t lid = kaapi_memory_asid_get_lid(cache->asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  kaapi_cache_entry_t* curr = list->end;
  kaapi_cache_entry_t* pcurr;

  while ((curr != 0) && (size >0))
  {
    pcurr = curr->prev;
    if (kaapi_memory_replica_is_notpinned(curr->mdi, lid)
     && kaapi_memory_replica_is_valid_excepton(curr->mdi, lid)
    )
    {
      kaapi_assert_debug( kaapi_memory_replica_is_allocated(curr->mdi, lid) );

      kaapi_memory_replica_unset_valid(curr->mdi, lid);
      kaapi_assert_debug( kaapi_memory_replica_is_valid_somewhere(curr->mdi) );

/* Remainder part of the protocol should be
      kaapi_mem_barrier();
      if (kaapi_memory_replica_under_xfer(curr->mdi)) abort eviction
  On the concurrent threads:
      kaapi_memory_replica_set_xfer(mdi, lidj); // lidj: lid that want to read value
      kaapi_mem_barrier();
      if (!kaapi_memory_replica_is_valid(curr->mdi,lid))
        redo transfer with new valid replica
*/
      kaapi_assert_debug( list == curr->mdi->replicas[lid]->cachelist );

      kaapi_memory_replica_unset_allocated(curr->mdi, lid);
      kaapi_assert( !kaapi_memory_replica_is_xfer(curr->mdi, lid) );

      size_t size_view = kaapi_memory_view_size( &curr->mdi->replicas[lid]->view );
      kaapi_assert_debug( curr->mdi->replicas[lid]->ptr.asid == device->asid );
      kaapi_memory_free(curr->mdi->replicas[lid]->ptr, size_view );
//printf("* evict data in list '%s' @:%p size:%lu\n", list == &cache->ro ? "ro" : "rw", curr->mdi->replicas[lid]->ptr.ptr, size_view );
      curr->mdi->replicas[lid]->ptr = kaapi_make_nullpointer(cache->asid);
      curr->mdi->replicas[lid]->cachelist = 0;
      curr->mdi->replicas[lid]->cacheentry = 0;
      if (size < size_view) size = 0;
      else size -= size_view;

      kaapi_assert_debug( cache->size_used >= size_view );
      cache->size_used -= size_view;

      /* erase entry from cache list */
      if (curr->next !=0) curr->next->prev = pcurr;
      else list->end = pcurr;
      if (curr->prev !=0) curr->prev->next = curr->next;
      else list->beg = curr->next;
      curr->prev = 0;
      curr->next = cache->freelist;
      cache->freelist = curr;
      if (size ==0) return 0;
    }

    curr = pcurr;
  }
  return size;
}




/* Initiate communication from device to the host.
   Wait completion of the communications should be done.
   On return.
*/
uint64_t kaapi_memory_writeback_all(
  kaapi_dsm_t* dsm,
  kaapi_address_space_id_t asid,
  kaapi_io_cbk_fnc_t cbk,
  void* arg0, void* arg1, void* arg2
)
{
  uint64_t send_msg = 0;
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  uint16_t lidhost = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lidhost < KAAPI_MEMORY_MAX_NODES);

  /* Only iterates on RW list where touched data should be found */
  kaapi_cache_list_t* list = &dsm->nodes[lid]->cache->rw;
  kaapi_cache_entry_t* curr = list->beg;
  kaapi_cache_entry_t* ncurr;

  while (curr != 0)
  {
    ncurr = curr->next;
    kaapi_assert(kaapi_memory_replica_is_allocated(curr->mdi, lidhost) );
    if (!kaapi_memory_replica_is_valid( curr->mdi, lidhost )
      && kaapi_memory_replica_is_valid( curr->mdi, lid ))
    {
#if KAAPI_DEBUG
      if (kaapi_memory_replica_is_valid( curr->mdi, lid == 1 ? 2 : 1 ))
        printf("*** also valid on other cache\n");
#endif
      int err = kaapi_dsm_prefetch_on( dsm, kaapi_local_asid, curr->mdi, cbk, arg0, arg1, arg2 );
      kaapi_assert((err ==0) || (err ==EINPROGRESS));
      if (err == EINPROGRESS) ++send_msg;
    }
    curr = ncurr;
  }
  return send_msg;
}


/* Evict some memory to make room for more allocations (at least of size size).
 * Return 0 in case of success, else ENOMEM
 */
static int kaapi_memory_cache_evict(
  kaapi_memory_device_t* device,
  kaapi_memory_cache_t* cache,
  size_t size
)
{
  if (size >0)
  {
    int cnt = 0;
    do {
      size = kaapi_memory_cache_evict_fromlist(device, cache, size, &cache->ro);
      if (size >0)
      {
        ++cnt;
        kaapi_offload_poll_devices();
      }
    } while ((size >0) && (cnt <32));
  }

  if (size >0)
  {
    int cnt = 0;
    do {
      size = kaapi_memory_cache_evict_fromlist(device, cache, size, &cache->rw);
      if (size >0)
      {
        ++cnt;
        kaapi_offload_poll_devices();
      }
    } while ((size >0) && (cnt <2));
  }


  /* here if size >0 then it could be necessary to write back valid bloc on the cache
     but not pinned.
  */

#if 0//KAAPI_DEBUG
  if (size !=0)
   _kaapi_memory_cache_print(cache);
#endif

  return size ==0 ? 0 : ENOMEM;
}


/* Force to invalidate all blocs of the cache
*/
int kaapi_memory_cache_invalidate_bloc(
  kaapi_memory_device_t* device,
  kaapi_memory_cache_t* cache,
  kaapi_metadata_info_t* mdi
)
{
  uint16_t lidhost = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lidhost < KAAPI_MEMORY_MAX_NODES);
  uint16_t lid = kaapi_memory_asid_get_lid(device->asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  if (lid == lidhost) return 0;

  if (cache ==0)
    cache = kaapi_the_dsm.nodes[lid]->cache;

  kaapi_assert( kaapi_memory_replica_is_allocated( mdi, lidhost ));
  kaapi_memory_replica_set_all_dirty_except(mdi, lidhost);

  size_t size = kaapi_memory_view_size(&mdi->replicas[lid]->view);

#if 1//KAAPI_DEBUG
  if (!(cache->size_used >= size))
    _kaapi_memory_cache_print(cache);
#endif
  kaapi_assert((cache->size_used >= size));

  if (mdi->replicas[lid]->ptr.ptr !=0)
  {
    kaapi_assert_debug(cache->size_used >= size);
    kaapi_assert( mdi->replicas[lid]->ptr.asid == device->asid );
    kaapi_memory_free(mdi->replicas[lid]->ptr, size);
    mdi->replicas[lid]->ptr.ptr = 0;
    cache->size_used -= size;
  }
  kaapi_memory_replica_unset_allocated(mdi, lid );
  kaapi_memory_replica_unset_valid(mdi, lid );
  kaapi_memory_replica_unset_pinned(mdi, lid );

  return 0;
}


/*
*/
static int kaapi_memory_cache_invalidate_fromlist(
  kaapi_memory_device_t* device,
  kaapi_memory_cache_t* cache,
  kaapi_cache_list_t* list
)
{
  uint16_t lidhost = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lidhost < KAAPI_MEMORY_MAX_NODES);
  uint16_t lid = kaapi_memory_asid_get_lid(cache->asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  if (lid == lidhost) return 0;

  kaapi_cache_entry_t* curr = list->beg;
  size_t size_cached = 0;

#if 0
  printf("%2i:: >>> [%s] size_limit:%lu, size_used:%lu: invalidate all data. list: %p, size: %lu\n", 
       lid, 
       __FUNCTION__, 
       cache->size_limit, cache->size_used, 
      (void*)list, _kaapi_memory_cache_sizelist(cache, list)
  );
#endif
  while (curr != 0)
  {
    size_t size = kaapi_memory_view_size(&curr->mdi->replicas[lid]->view);
    size_cached += size;
    kaapi_memory_cache_invalidate_bloc( device, cache, curr->mdi );

    kaapi_cache_entry_t* ncurr = curr->next;
    curr->mdi->replicas[lid]->cacheentry = 0;
    curr->mdi->replicas[lid]->cachelist = 0;
    curr->next = cache->freelist;
    curr = ncurr;
    cache->freelist = curr;
  }
  list->beg = list->end = 0;
#if 0
  printf("%2i:: <<< [%s] size_limit:%lu, size_used:%lu: size invalidated data: %lu\n", lid, __FUNCTION__, cache->size_limit, cache->size_used, size_cached);
#endif
  return 0;
}


/* Invalidation of all entries in the cache asid.
   Memory blocs are reclaimed.
*/
int kaapi_memory_invalidate_cache( kaapi_address_space_id_t asid )
{
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  int err;
  kaapi_memory_cache_t* cache = kaapi_the_dsm.nodes[lid]->cache;
  kaapi_memory_device_t* device = kaapi_the_dsm.nodes[lid]->device;
  if (cache !=0)
  {
    err = kaapi_memory_cache_invalidate_fromlist(device, cache, &cache->ro);
    if (err) return err;
    cache->ro.beg = cache->ro.end = 0;
    err = kaapi_memory_cache_invalidate_fromlist(device, cache, &cache->rw);
    if (err) return err;
    cache->rw.beg = cache->rw.end = 0;

    kaapi_assert_debug(cache->size_used == 0);
  }
  return 0;
}

/*
*/
static void kaapi_memory_freehashmap( kaapi_hashmap_t* ht, kaapi_address_space_id_t asid )
{
  int err;
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  if (lid==0)
  {
    kaapi_hashentries_t* entry;
    for (uint32_t i = 0; i< kaapi_hashmap_sizeentries(ht); ++i)
    {
      do {
        entry = _pop_hashmap_entry(ht, i);
        if (entry ==0) break;
        kaapi_metadata_info_t* mdi = KAAPI_HASHENTRIES_GET(entry, kaapi_metadata_info_t*);
//#warning "How it possible that mdi ==0 here ?"
        if (mdi !=0)
        {
          _kaapi_dsm_deallocate_replica(&kaapi_the_dsm, mdi);
          free(mdi);
          KAAPI_HASHENTRIES_SET(entry, 0, kaapi_metadata_info_t*);
        }
      } while (1);
    }
  }
  err = kaapi_hashmap_clear(ht);
}

/* -------------------------------------------------------------------------- */
/* High level function: do memory copy
   Return 0 if the operation successfully completes before
   returning to the caller.
   It returns EINPROGRESS if the operation does not complete.
   Else it returns error code.
   Depending on the source and dest pointer, the function dispatch the call
   to the corresponding device that is not pure host device (host device
   cannot talk with true device while true device can transfer data between
   the device and the host).
*/
int kaapi_memory_copy_async(
    kaapi_pointer_t dest, const kaapi_memory_view_t* view_dest,
    kaapi_pointer_t src, const kaapi_memory_view_t* view_src,
    kaapi_io_cbk_fnc_t cbk,
    void* arg0, void* arg1, void* arg2
)
{
  kaapi_memory_device_t* dest_dev = kaapi_memory_device_get( dest.asid );
  kaapi_memory_device_t* src_dev = kaapi_memory_device_get( src.asid );
  kaapi_memory_device_t* selected = (
    (dest_dev ==0) || (kaapi_memory_asid_get_arch(dest_dev->asid) == KAAPI_PROC_TYPE_HOST) ?
      src_dev : dest_dev
  );
  return selected->f_copy(
    selected,
    dest, view_dest,
    src, view_src,
    cbk, arg0, arg1, arg2
  );
}


/*
*/
int kaapi_memory_copy(
    kaapi_pointer_t dest, const kaapi_memory_view_t* view_dest,
    kaapi_pointer_t src, const kaapi_memory_view_t* view_src
)
{
  int err = kaapi_memory_copy_async( dest, view_dest, src, view_src, 0, 0, 0, 0 );
  if (err == EINPROGRESS)
  {
    kaapi_memory_device_t* dest_dev = kaapi_memory_device_get( dest.asid );
    kaapi_memory_device_t* src_dev __attribute__((unused)) = kaapi_memory_device_get( src.asid );
    dest_dev->f_memsync( dest_dev, 0);
  }

  return err;
}


/* -------------------------------------------------------------------------- */

int kaapi_dsm_init( void )
{
  int err = 0;
  memset( &kaapi_the_dsm.nodes, 0, sizeof(kaapi_dsm_node_t*)*KAAPI_MEMORY_MAX_NODES);
#if KAAPI_DEBUG_MEMORY_ALLOC
  kaapi_hashmap_init(&free_ptr_ht, free_mapentries, KAAPI_SIZE_DBG_MAP_ALLOC, 0);
#endif
#if MEM_ALLOC_FREELIST 
  if (getenv("KAAPI_VERBOSE")) 
    printf("[xkaapi] use free list allocator\n" );
#endif


  /* fill virtual node 0 as the host node */
  kaapi_dsm_node_t* node = (kaapi_dsm_node_t*)malloc(sizeof(kaapi_dsm_node_t));
  if (node ==0) return ENOMEM;
  kaapi_atomic_initlock(&node->lock);
  err = kaapi_hashmap_init(&node->ht, node->mapentries, KAAPI_SIZE_DSM_MAP, 0);
  if (err) return err;
  node->device = 0;
  node->cache = 0;
  kaapi_the_dsm.nodes[0] = node;
  return err;
}


/*
*/
int kaapi_dsm_finalize( void )
{
  int err;
  kaapi_memory_freehashmap( &kaapi_the_dsm.nodes[0]->ht, kaapi_local_asid );

  err = kaapi_memory_cache_destroy( kaapi_the_dsm.nodes[0]->cache);
  if (err) return err;

  err = kaapi_hashmap_clear(&kaapi_the_dsm.nodes[0]->ht);
  if (err) return err;
  err = kaapi_hashmap_destroy(&kaapi_the_dsm.nodes[0]->ht);
  if (err) return err;
  kaapi_atomic_destroylock(&kaapi_the_dsm.nodes[0]->lock);
  free(kaapi_the_dsm.nodes[0]);
  kaapi_the_dsm.nodes[0] = 0;
  return 0;
}


/* Allocate the asid for the newly registered device
*/
int kaapi_dsm_register_device(
    kaapi_dsm_t* dsm,
    kaapi_memory_device_t* device,
    int arch
)
{
  int err;
  if (arch == KAAPI_PROC_TYPE_HOST)
    device->asid = kaapi_local_asid;
  else
    device->asid = kaapi_memory_create_asid(
      0, /* global id */
      KAAPI_ATOMIC_INCR(&kaapi_dsm_asid_lid), /* lid */
      arch
    );

  device->freelist_bloc = 0;
  device->freelist_metabloc  = 0;
  
  uint16_t lid = kaapi_memory_asid_get_lid(device->asid);
  if (lid >= KAAPI_MEMORY_MAX_NODES)
  {
    printf("*** Number of requested memory nodes is upper than defined limit\n"
           "    Please augment 'KAAPI_MEMORY_MAX_NODES' in kaapi_memory.h\n"
    );
    return ENOMEM;
  }
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  if (dsm->nodes[lid] !=0) return 0; /* already initialized */

  kaapi_dsm_node_t* node = (kaapi_dsm_node_t*)malloc(sizeof(kaapi_dsm_node_t));
  if (node ==0) return ENOMEM;
  err = kaapi_hashmap_init(&node->ht, node->mapentries, KAAPI_SIZE_DSM_MAP, 0);
  if (err) return err;
  node->device = device;
  node->cache = kaapi_memory_cache_init(device, device->asid, 0);
  if (err) return ENOMEM;
  kaapi_the_dsm.nodes[lid] = node;

  return 0;
}


/* Unregister a device to the dsm
*/
int kaapi_dsm_unregister_device(
    kaapi_dsm_t* dsm,
    kaapi_memory_device_t* device
)
{
  int err;
  uint16_t lid = kaapi_memory_asid_get_lid(device->asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  /* node 0 is processed in dsm_finalize */
  if (lid ==0) return 0;

  /* suppress device */
  kaapi_dsm_node_t* node = dsm->nodes[lid];
  dsm->nodes[lid] = 0;

  err = kaapi_memory_cache_destroy( node->cache);
  if (err) return err;
  node->device = 0;
  node->cache = 0;
  err = kaapi_hashmap_clear(&node->ht);
  if (err) return err;
  err = kaapi_hashmap_destroy(&node->ht);
  if (err) return err;
  free(node);

  return 0;
}


/*
*/
void kaapi_dsm_print_mdi(
    const char* fname,
    const kaapi_metadata_info_t* mdi
)
{
 printf("[%s MDI] valid=[", fname);
 int valid = KAAPI_ATOMIC_READ(&mdi->valid);
 while (valid !=0)
 {
   int lid= __builtin_ffsll( valid);
   if (lid== 0 ) break;
   --lid;
   printf("%i/asid=%i, @=%p ", lid, 
     kaapi_memory_asid_get_lid(mdi->replicas[lid]->ptr.asid),
     (void*)mdi->replicas[lid]->ptr.ptr);
   valid &= ~(1<<lid);
 }

 printf("], alloc=[");
 int alloc = KAAPI_ATOMIC_READ(&mdi->alloc);
 while (alloc !=0)
 { 
   int lid= __builtin_ffsll( alloc);
   if (lid== 0 ) break;
   --lid;
   printf("%i/asid=%i @=%p ", lid, 
     kaapi_memory_asid_get_lid(mdi->replicas[lid]->ptr.asid),
     (void*)mdi->replicas[lid]->ptr.ptr);
   alloc &= ~(1<<lid);
 }
 printf("]\n");
}

/* Allocate a new main meta data information with replica on the host node.
*/
static inline kaapi_data_replica_t* _kaapi_new_replica(
    kaapi_address_space_id_t asid,
    const kaapi_memory_view_t* view
)
{
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  /* allocate entry in lid0 */
  kaapi_data_replica_t* kdr = (kaapi_data_replica_t*)malloc(sizeof(kaapi_data_replica_t));
  kdr->ptr        = kaapi_make_nullpointer( asid );
  kdr->view       = *view;
  kdr->cbk.iocbk.fnc = 0;
  kdr->cbk.next   = 0;
  kdr->cachelist  = 0;
  kdr->cacheentry = 0;
  KAAPI_ATOMIC_WRITE(&kdr->pinned, 0);  /* one reference count to the application data */
  return kdr;
}



/* Allocate a new main meta data information with replica on the host node.
*/
static kaapi_metadata_info_t* _kaapi_new_mdi(
    void* ptr,
    const kaapi_memory_view_t* view
)
{
  uint16_t lid0 = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lid0 < KAAPI_MEMORY_MAX_NODES);

  kaapi_metadata_info_t* mdi = (kaapi_metadata_info_t*)malloc(sizeof(kaapi_metadata_info_t));
  memset( &mdi->replicas, 0, sizeof(mdi->replicas));

  /* allocate replica and entry for lid0 */
  kaapi_data_replica_t* kdr = _kaapi_new_replica( kaapi_local_asid, view );
  kdr->ptr = kaapi_make_pointer(ptr, kaapi_local_asid);
  KAAPI_ATOMIC_WRITE(&kdr->pinned, 1);  /* one reference count to the application data */
  mdi->replicas[lid0] =  kdr;
  KAAPI_ATOMIC_WRITE(&mdi->alloc,  1ULL<<lid0);
  KAAPI_ATOMIC_WRITE(&mdi->valid,  1ULL<<lid0);
  KAAPI_ATOMIC_WRITE(&mdi->xfer,   0ULL);
  return mdi;
}


/* If defined, then interact with findaccess_on node and
   results (dgemm test) are incorrect == should separate insertion
   of the mdi from the allocation of replica information with the view !!!
*/
int kaapi_dsm_debug_name(
    kaapi_dsm_t* dsm,
    void *ptr,
    const kaapi_memory_view_t* view,
    const char* name
)
{
printf("Do not use function. Abort\n");
abort();

  uint16_t lid0 = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lid0 < KAAPI_MEMORY_MAX_NODES);
  kaapi_metadata_info_t* mdi;
  kaapi_hashentries_t* entry;
  entry = kaapi_hashmap_find( &dsm->nodes[lid0]->ht, ptr );
  if (entry ==0)
  {
    mdi = _kaapi_new_mdi( ptr, view );
    entry = kaapi_hashmap_insert( &dsm->nodes[lid0]->ht, ptr );
    KAAPI_HASHENTRIES_SET(entry, mdi, kaapi_metadata_info_t*);
  }
  else
    mdi = KAAPI_HASHENTRIES_GET(entry, kaapi_metadata_info_t*);
  mdi->debug_info = strdup(name);
  return 0;
}


/* This function first look in the per device hashmap to retreive the data.
   If it does not exist, then it search in the global dsm hashmap.
   On return:
    - mdi and the replica for device lid are allocated
    - replica for host node is allocated.
*/
kaapi_metadata_info_t* kaapi_dsm_findaccess_on_node(
      kaapi_dsm_t* dsm,
      kaapi_address_space_id_t asid,
      int createflag,
      kaapi_access_t* a,
      const kaapi_memory_view_t* view
)
{
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  uint16_t lid0 = kaapi_memory_asid_get_lid(kaapi_local_asid);
  kaapi_assert_debug(lid0 < KAAPI_MEMORY_MAX_NODES);

  kaapi_metadata_info_t* mdi = 0;
  kaapi_hashentries_t* entry;
  if (lid == (uint16_t)-1) lid = lid0;

  /* revoir ce point */
#if 1
  kaapi_atomic_lock( &dsm->nodes[lid0]->lock );
  entry = kaapi_hashmap_find( &dsm->nodes[lid0]->ht, a->data );
  kaapi_atomic_unlock( &dsm->nodes[lid0]->lock );
#else
  kaapi_atomic_lock( &dsm->nodes[lid0]->lock );
  entry = kaapi_hashmap_find( &dsm->nodes[lid]->ht, a->data );
  if ((entry ==0) && (lid != lid0))
    entry = kaapi_hashmap_find( &dsm->nodes[lid0]->ht, a->data );
  kaapi_atomic_unlock( &dsm->nodes[lid0]->lock );
#endif

  if ((entry ==0) && !createflag)
    return 0;

  if ((entry ==0) && createflag) /* allocate: both in nodes[0] and nodes[lid] */
  {
    kaapi_assert_debug( view );
    kaapi_atomic_lock( &dsm->nodes[lid0]->lock );
    entry = kaapi_hashmap_findinsert( &dsm->nodes[lid0]->ht, a->data );
    mdi = KAAPI_HASHENTRIES_GET(entry, kaapi_metadata_info_t*);
    if (mdi ==0)
    {
      mdi = _kaapi_new_mdi( a->data, view );
      KAAPI_HASHENTRIES_SET(entry, mdi, kaapi_metadata_info_t*);
      kaapi_atomic_unlock( &dsm->nodes[lid0]->lock );
      goto return_value;
    }
    kaapi_atomic_unlock( &dsm->nodes[lid0]->lock );
  }

  //
  {
    /* find entry. may be view differ : old entry.
     * here may be it would be best to have cache flush or cache invalidate that 
     * flush all entries in the cache    
     */
    mdi = KAAPI_HASHENTRIES_GET(entry, kaapi_metadata_info_t*);
#if KAAPI_DEBUG
    if ((uintptr_t)a->data != mdi->replicas[0]->ptr.ptr)
    {
      printf("Cache------ 1\n");
      _kaapi_memory_cache_print(kaapi_the_dsm.nodes[1]->cache);
      printf("Cache------ 2\n");
      _kaapi_memory_cache_print(kaapi_the_dsm.nodes[2]->cache);

      kaapi_hashmap_t* ht = &kaapi_the_dsm.nodes[0]->ht;
      uint16_t lid0 = kaapi_memory_asid_get_lid(kaapi_local_asid);
      kaapi_hashentries_t* entry;
      for (uint32_t i = 0; i< kaapi_hashmap_sizeentries(ht); ++i)
      {
        entry = _get_hashmap_entry(ht, i);
        while (entry !=0)
        {
          kaapi_metadata_info_t*mdi = KAAPI_HASHENTRIES_GET(entry, kaapi_metadata_info_t*);
          if (mdi !=0) kaapi_dsm_print_mdi("mdi", mdi);
          entry = entry->next;
        }
      }
    }
#endif
    kaapi_assert( (uintptr_t)a->data == (uintptr_t)mdi->replicas[0]->ptr.ptr );

    /* make sure view correspond */
    kaapi_assert_debug(
      kaapi_memory_view_size(view) == kaapi_memory_view_size(&mdi->replicas[lid0]->view)
    );
    if (createflag) 
      mdi->replicas[lid0]->view = *view;

      //kaapi_assert_debug(kaapi_memory_view_size(view) <= TILE_SIZE);
// TODO here : what is data already in the cache ?
// update cache->size in order to pass assert
  }

return_value:
  /* */
  a->mdi = mdi;
  /* allocate replica but not the memory block */
  if (createflag && (mdi->replicas[lid] ==0))
  {
    mdi->replicas[lid] = _kaapi_new_replica( asid, view);
  }

  return mdi;
}


/* Allocate a replica on lid:
   - if memory is already allocated and has the same size then try to reuse it
   - else new data is allocated from the memory allocator
   On return the alloc bit field of mdi is updated to reflect that data on asid is
   allocated.
   Return value is:
     - 0 in case of success
     - else possible error:
      - ENOMEM: cannot allocate memory on the device
*/
static int _kaapi_dsm_allocate_replica(
    kaapi_dsm_t* dsm,
    kaapi_metadata_info_t* mdi,
    kaapi_address_space_id_t asid,
    kaapi_memory_view_t* view
)
{
  kaapi_assert_debug( view );
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);
  uint16_t lid0 = kaapi_memory_asid_get_lid(kaapi_local_asid);
  size_t size = kaapi_memory_view_size(view);

  /* */
  kaapi_data_replica_t* kdr = mdi->replicas[lid];
  kaapi_assert_debug(kdr !=0);

  if (size != kaapi_memory_view_size(&kdr->view))
  {
    kaapi_memory_free(kdr->ptr, kaapi_memory_view_size(&kdr->view));
    kdr->ptr = kaapi_make_nullpointer(asid);
    kaapi_memory_replica_unset_valid(mdi,lid);
    kaapi_memory_replica_unset_allocated(mdi,lid);
    /* update cache info if any */
    if (kdr->cacheentry)
    {
      kaapi_memory_cache_t* cache = kaapi_the_dsm.nodes[lid]->cache;
      cache->size_used -= kaapi_memory_view_size(&kdr->view);
    }
  }
  kdr->view = *view;

  if (!kaapi_memory_replica_is_allocated(mdi, lid))
  {
    kaapi_assert_debug( kaapi_pointer_isnull( kdr->ptr ) );
    kaapi_assert_debug( !kaapi_memory_replica_is_valid(mdi,lid) );
    int retry_cnt = 0;
    
    kdr->ptr = kaapi_memory_alloc( asid, size );
    while (kaapi_pointer_isnull( kdr->ptr ))// && (++retry_cnt <32))
    {
      int err = kaapi_memory_cache_evict(dsm->nodes[lid]->device, dsm->nodes[lid]->cache, size);
      if (err == 0)
        kdr->ptr = kaapi_memory_alloc( asid, size );
      if (err == ENOMEM || kaapi_pointer_isnull( kdr->ptr ))
        kaapi_offload_poll_device( dsm->nodes[lid]->device->device );
      retry_cnt++;
    }
  }

  if (kaapi_pointer_isnull( kdr->ptr ))
    return ENOMEM;

  /* on each device data is always a copy of the original data, so compact the view */
  if (lid != lid0)
    kaapi_memory_view_reallocated(&kdr->view);
  kaapi_memory_replica_set_allocated(mdi,lid);

  return 0;
}


/*
*/
static int _kaapi_dsm_deallocate_replica(
    kaapi_dsm_t* dsm,
    kaapi_metadata_info_t* mdi
)
{
  /* dispatch over all caches / memory devices */
  for (uint16_t lid=0; lid<KAAPI_MEMORY_MAX_NODES; ++lid)
  {
    if (mdi->replicas[lid] ==0) continue;
    if (kaapi_memory_replica_is_allocated(mdi, lid ))
    {
      kaapi_memory_replica_unset_allocated(mdi, lid);
      //kaapi_memory_device_t* device = kaapi_memory_device_get(mdi->replicas[lid]->ptr.asid);
      //if (device ==0) break;
      if (lid !=0)
      {
        size_t size_view = kaapi_memory_view_size( &mdi->replicas[lid]->view );
        kaapi_memory_free(mdi->replicas[lid]->ptr, size_view );
      }
      mdi->replicas[lid]->ptr = kaapi_make_nullpointer(0);
      mdi->replicas[lid]->cachelist = 0;
      kaapi_cache_entry_t* entry = mdi->replicas[lid]->cacheentry;
      if (entry !=0)
      {
        kaapi_memory_cache_t* cache = dsm->nodes[lid]->cache;
        mdi->replicas[lid]->cacheentry = 0;
        entry->next = cache->freelist;
        cache->freelist = entry;
      }
    } 
    free(mdi->replicas[lid]);
    mdi->replicas[lid] = 0;
  }
  return 0;
}



/* Take the first valid lid to make copy to dest_asid.
   If several possibility should return the "best" lid.
*/
uint16_t _kaapi_get_valid_lid(
  kaapi_metadata_info_t* mdi, kaapi_address_space_id_t dest_asid, int mark
)
{
  uint16_t lid0, lid_dest;
  int valid_bit;
  int lid_valid;

  lid0 = kaapi_memory_asid_get_lid(kaapi_local_asid);
  lid_dest = kaapi_memory_asid_get_lid(dest_asid);

  /* choose valid copies to move data to device
     - warning: only replica with valid bit, not in transit are
     ensured to be send to the device
  */
  //#warning "Here store topology between GPUs through a better_peer() function"
  //#warning "Here protocol is not concurrent with eviction !!!! "
reload:
  valid_bit= KAAPI_ATOMIC_READ(&mdi->valid);

#if 1 // Set 1 if try to return first GPU ressource before the host CPU
  valid_bit &= ~(1<<lid0);

  /* if ==0 => lid0 is the only valid ressource */
  if (valid_bit ==0)
  {
    if (mark)
    {
      kaapi_memory_replica_mark_pinned(mdi, lid0);
      int cnt = KAAPI_ATOMIC_READ(&mdi->replicas[lid0]->pinned);
      kaapi_assert_debug( cnt >1 );
    }
    return lid0;
  }
#endif

#if 0
#if 0 // for DGX1
// TOPO DGX1
/* 4 levels of affinity. LID=0 == CPU, so shift value +1*/
#define HLEVEL_AFFINITY 4
#define BIT(x) (1<<(1+(x)))
static int affinity[9][HLEVEL_AFFINITY] = {
  { 0, ~0,         0,             0,                   0},
  { 0, BIT(3)|BIT(4), BIT(1)|BIT(2), BIT(5)|BIT(6)|BIT(7)},
  { 1, BIT(2)|BIT(5), BIT(0)|BIT(3), BIT(4)|BIT(6)|BIT(7)},
  { 2, BIT(1)|BIT(3), BIT(0)|BIT(6), BIT(4)|BIT(5)|BIT(7)},
  { 3, BIT(0)|BIT(2), BIT(1)|BIT(7), BIT(4)|BIT(5)|BIT(6)},
  { 4, BIT(0)|BIT(7), BIT(5)|BIT(6), BIT(1)|BIT(2)|BIT(3)},
  { 5, BIT(1)|BIT(6), BIT(4)|BIT(7), BIT(0)|BIT(2)|BIT(3)},
  { 6, BIT(5)|BIT(7), BIT(2)|BIT(4), BIT(0)|BIT(1)|BIT(3)},
  { 7, BIT(4)|BIT(6), BIT(3)|BIT(5), BIT(0)|BIT(1)|BIT(2)}
};

#else // for BLAISE

// TOPO BLAISE
/* 3 levels of affinity. LID=0 == CPU, so shift value +1*/
#define BIT(x) (1<<(1+(x)))
#define HLEVEL_AFFINITY 3
static int affinity[5][HLEVEL_AFFINITY] = {
  { 0, ~0,         0,       },
  { 0, BIT(3), BIT(1)|BIT(2)},
  { 1, BIT(2), BIT(0)|BIT(3)},
  { 2, BIT(1), BIT(0)|BIT(3)},
  { 3, BIT(0), BIT(1)|BIT(2)}
};
#endif

  for (int i=1; i<HLEVEL_AFFINITY; ++i)
  {
    if (valid_bit & affinity[lid_dest][i])
    {
redo1:
      lid_valid = __builtin_ffsll( valid_bit & affinity[lid_dest][i] );
      if (lid_valid ==0) { ++i; continue; }
      --lid_valid;
      valid_bit &= ~(1<<lid_valid);
      if (mark)
        kaapi_memory_replica_mark_pinned(mdi, lid_valid);
      if (!kaapi_memory_replica_is_valid(mdi, lid_valid))
      {
        if (mark)
          kaapi_memory_replica_unmark_pinned(mdi, lid_valid);
        goto redo1; // next valid at same level ?
      }
      kaapi_assert_debug(lid_valid < KAAPI_MEMORY_MAX_NODES);
      //printf("*** Return from level: %i\n", i);
      return lid_valid;
    }
  }
  valid_bit=0;
  goto reload;

#else

redo:
#if 1 // RANDOM
{
  int rnd = rand() % __builtin_popcountll(valid_bit);
  for (int i=0; i<rnd; ++i)
  {
    lid_valid = __builtin_ffsll( valid_bit );
    kaapi_assert_debug( lid_valid != 0);
    --lid_valid;
    valid_bit &= ~(1<<lid_valid);
  }
}
#endif

  lid_valid = __builtin_ffsll( valid_bit );
  if (lid_valid == 0) goto reload;
  --lid_valid;
  valid_bit &= ~(1<<lid_valid);
  if (mark)
    kaapi_memory_replica_mark_pinned(mdi, lid_valid);
  if (!kaapi_memory_replica_is_valid(mdi, lid_valid))
  {
    if (mark)
      kaapi_memory_replica_unmark_pinned(mdi, lid_valid);
    goto redo;
  }
  kaapi_assert_debug(lid_valid < KAAPI_MEMORY_MAX_NODES);
  return lid_valid;
#endif
}


#if KAAPI_DEBUG
kaapi_atomic_t pending_xfer={0};
kaapi_atomic_t received_xfer={0};
#endif
/* Activate the list of callbacks for a given replica
*/
static void callback_activate_replica_on_receive_cbk(
    kaapi_io_status_t status,
    kaapi_io_stream_t* ios,
    void* arg0, void* arg1, void* arg2
)
{
  kaapi_metadata_info_t* mdi = (kaapi_metadata_info_t*)arg0;
  kaapi_address_space_id_t asid = (kaapi_address_space_id_t)arg1;
  uint16_t lid_valid = (uint16_t)(uintptr_t)arg2;
  kaapi_assert_debug(lid_valid < KAAPI_MEMORY_MAX_NODES);
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  /* mark data received : unpinned source data and set validity */
  kaapi_memory_replica_set_valid(mdi, lid);
  kaapi_memory_replica_unset_xfer(mdi, lid);
  kaapi_memory_replica_unmark_pinned( mdi, lid_valid );
#if KAAPI_DEBUG
  KAAPI_ATOMIC_INCR(&received_xfer);
#endif

  /* activate cbk */
  kaapi_data_replica_cbk_t* drc0 = &mdi->replicas[lid]->cbk;
  kaapi_data_replica_cbk_t* drc = drc0;

  do {
    if (drc->iocbk.fnc !=0)
    {
      drc->iocbk.fnc( status, ios,
          drc->iocbk.arg[0], drc->iocbk.arg[1], drc->iocbk.arg[2]
      );
      drc->iocbk.fnc = 0;
    }
    kaapi_data_replica_cbk_t* drcn = drc->next;
    drc->next = 0;
    if (drc != drc0) free(drc);
    drc = drcn;
  } while (drc !=0);

  kaapi_assert(drc0->next == 0);
  kaapi_assert(drc0->iocbk.fnc == 0);
}


/* fetch data on node asid if data is not valid on it.
   Look if data is already present or not. If present, then the call returns 0.
   If data is not present a message to get it back is sent except if already under transit.
   Return 0: data is already present
   Return EINVAL: data is already valid
   Return EINPROGRESS if a message is send and data is under transfer
*/
static int kaapi_dsm_fetch_on(
      kaapi_dsm_t* dsm,
      kaapi_address_space_id_t asid,
      kaapi_metadata_info_t* mdi,
      kaapi_io_cbk_fnc_t cbk,
      void* arg0, void* arg1, void* arg2
)
{
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  if (kaapi_memory_replica_is_valid(mdi, lid)) return EINVAL;

  /* get a valid replica with good locality. Note that on return, the replicas[lid_valid]
     is pinned until call back callback_activate_replica_on_receive_cbk is called
     (value 1 in the call).
  */
  uint16_t lid_valid = _kaapi_get_valid_lid(mdi, asid, 1);

  /* chain callbacks together
     TODO: better usage of the memory in order to avoid malloc here.
  */
  if (cbk !=0)
  {
    kaapi_data_replica_cbk_t* drc;
    kaapi_data_replica_t* r = mdi->replicas[lid];
    if (r->cbk.iocbk.fnc == 0)
      drc = &r->cbk;
    else
      drc = malloc(sizeof(kaapi_data_replica_cbk_t));
    drc->iocbk.fnc = cbk;
    drc->iocbk.arg[0] = arg0;
    drc->iocbk.arg[1] = arg1;
    drc->iocbk.arg[2] = arg2;
    drc->next = 0;
    if (drc != &r->cbk)
    {
      drc->next = r->cbk.next;
      r->cbk.next = drc;
    }
  }

  /* chaining just above is not protected against concurrent access because only
     the current thread can make progress of communications and calls callbacks
  */
  if (kaapi_memory_replica_is_xfer(mdi, lid))
  {
    /* callback was link; nothing else to do */
    return EINPROGRESS;
  }

  kaapi_data_replica_t* src_replica = mdi->replicas[lid_valid];
  kaapi_data_replica_t* dest_replica = mdi->replicas[lid];
  kaapi_assert_debug( src_replica->ptr.ptr != 0);
  kaapi_assert_debug( dest_replica->ptr.ptr != 0);

  /* do not have concurrent access to device structure : one thread manages each device */
  kaapi_assert_debug( !kaapi_memory_replica_is_valid(mdi, lid) );
  kaapi_assert_debug( !kaapi_memory_replica_is_xfer(mdi, lid) );
  kaapi_assert_debug(kaapi_memory_view_size(&dest_replica->view) == kaapi_memory_view_size(&src_replica->view));

  kaapi_memory_replica_set_xfer(mdi, lid);
#if KAAPI_DEBUG
  KAAPI_ATOMIC_INCR(&pending_xfer);
#endif

  int err = kaapi_memory_copy_async(
      dest_replica->ptr, &dest_replica->view,
      src_replica->ptr, &src_replica->view,
      callback_activate_replica_on_receive_cbk,
      mdi, (void*)asid, (void*)(uintptr_t)lid_valid
  );
  kaapi_assert((err ==0) || (err ==EINPROGRESS));
  return err;
}


/* ~ to pin/unpin memory location
   may return ENOMEM if no more memory on the device
*/
int kaapi_dsm_acquire_data(
      kaapi_dsm_t* dsm,
      kaapi_address_space_id_t asid,
      kaapi_task_t* task,
      kaapi_access_mode_t mp,
      kaapi_metadata_info_t* mdi,
      kaapi_io_cbk_fnc_t cbk,
      void* arg0, void* arg1, void* arg2
)
{
  int err = 0;
  uint16_t lid0 = kaapi_memory_asid_get_lid(kaapi_local_asid);
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  /* pin the replica for the task */
  kaapi_memory_replica_mark_pinned( mdi, lid );

  /* do allocation if any. Use view on 0 to determine size and layout */
  err = _kaapi_dsm_allocate_replica( dsm, mdi, asid, &mdi->replicas[lid0]->view );

  /* here on return there is possible error if device memory is full (ENOMEM).
     TODO: process this error
  */
  if (err !=0) return err;

  /* if read then make copy on device->memorydevice.asid */
  if (KAAPI_ACCESS_IS_READ(mp))
  {
    /* difficult to integrate this test into fetch_on.
       because we need to know before fetch if the data will be received after
       in order to inc the waiting counter of the task.
    */
    if (!kaapi_memory_replica_is_valid(mdi, lid))
    {
      /* need aync comm -> increment waiting counter */
      if (task)
        KAAPI_ATOMIC_INCR(&task->wc);

      err = kaapi_dsm_fetch_on( dsm, asid, mdi, cbk, arg0, arg1, arg2 );
    }

    kaapi_memory_cache_touch( dsm, lid, KAAPI_ACCESS_MODE_R, mdi );
  }
  if (KAAPI_ACCESS_IS_WRITE(mp))
  {
    /* Putting here set_all_dirty is correct because there is no
       concurrency between the end of the tasks and reads thanks to data flow constraints.
       Nevertheless, putting all_dirty while the task is not yet executed will favor
       extra communication from valid data. Hence it would be preferable to delay the
       date where set_all_dirty is called even at the cost of an extra synchronisation to
       wait until the data on the on the fly data transfers.
    */
    kaapi_memory_replica_set_all_dirty_except( mdi, lid );

    /* touch entry in cache as write access */
    kaapi_memory_cache_touch( dsm, lid, KAAPI_ACCESS_MODE_W, mdi );
  }

  return err;
}



/*
*/
int kaapi_dsm_release_data(
      kaapi_metadata_info_t* mdi,
      kaapi_address_space_id_t asid,
      kaapi_access_t* a
)
{
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  kaapi_assert( a->mdi == mdi );
  kaapi_memory_replica_unmark_pinned( mdi, lid );
if (lid ==0)
{
      kaapi_assert_debug( KAAPI_ATOMIC_READ(&mdi->replicas[0]->pinned) >1 );
}

  /* here is m == write and a->next ==0 => back to the host memory ? */
  return 0;
}


/* May returns:
     EINPROGRESS : if data not yet on asid
     ENOMEM : not possible to allocate replica
*/
int kaapi_dsm_prefetch_on(
      kaapi_dsm_t* dsm,
      kaapi_address_space_id_t asid,
      kaapi_metadata_info_t* mdi,
      kaapi_io_cbk_fnc_t cbk,
      void* arg0, void* arg1, void* arg2
)
{
  uint16_t lid = kaapi_memory_asid_get_lid(asid);
  kaapi_assert_debug(lid < KAAPI_MEMORY_MAX_NODES);

  /* if not allocated: do allocation. Use view on 0 to determine size and layout */
  int err = _kaapi_dsm_allocate_replica( dsm, mdi, asid, &mdi->replicas[0]->view );
  if (err !=0) return err;

  if (!kaapi_memory_replica_is_valid(mdi, lid))
    return kaapi_dsm_fetch_on( dsm, asid, mdi,
        cbk, arg0, arg1, arg2
    );
  return 0;
}


/* -------------------------------------------------------------------------- */
/*
*/
void kaapi_memory_synchronize(void)
{
#if 0 /* to debug */
  uint16_t lid = 1;
  kaapi_memory_cache_t* cache = kaapi_the_dsm.nodes[lid]->cache;
  kaapi_memory_device_t* device = kaapi_the_dsm.nodes[lid]->device;
  if (cache !=0)
    _kaapi_memory_cache_print(cache);
#endif

#if KAAPI_USE_OFFLOAD
  kaapi_offload_synchronize();
#endif
  kaapi_mem_barrier();
}

/*
*/
int kaapi_memory_invalidate_caches(void)
{
#if KAAPI_USE_OFFLOAD
  kaapi_offload_invalidate_caches();
#endif
  //kaapi_mem_barrier();
  kaapi_memory_freehashmap( &kaapi_the_dsm.nodes[0]->ht, kaapi_local_asid );
#if CHECK
{
    kaapi_hashmap_t* ht = &kaapi_the_dsm.nodes[0]->ht;
    uint16_t lid0 = kaapi_memory_asid_get_lid(kaapi_local_asid);
    kaapi_hashentries_t* entry;
    for (uint32_t i = 0; i<kaapi_hashmap_sizeentries(ht); ++i)
    {
      entry = _get_hashmap_entry(ht, i);
      if (entry !=0)
      {
        printf("Error: non null entry %i after clearing hashmap\n",i);
        while (entry !=0)
        {
          kaapi_metadata_info_t*mdi = KAAPI_HASHENTRIES_GET(entry, kaapi_metadata_info_t*);
          printf("\tmdi: %p\n", mdi);
          entry = entry->next;

        }
      }
    }
}
#endif

  return 0;
}

/*
*/
struct kaapi_memgroup {
  kaapi_atomic_t counter;
#if 0 // TODO for non active waiting loop
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
  int iswaiting;
#endif
};

/*
*/
int kaapi_memory_group_init( kaapi_memgroup_t* grp)
{
  KAAPI_ATOMIC_WRITE(&grp->counter, 0);
#if 0
  kaapi_assert(0 == pthread_mutex_init(&grp->mutex, 0));
  kaapi_assert(0 == pthread_cond_init(&grp->cond, 0));
  grp->iswaiting = 0;
#endif
  return 0;
}

/*
*/
static void callback_signal_grp_on_receive_cbk(
    kaapi_io_status_t status,
    kaapi_io_stream_t* ios,
    void* arg0, void* arg1, void* arg2
)
{
  kaapi_memgroup_t* grp    = (kaapi_memgroup_t*)arg0;
  kaapi_metadata_info_t* mdi __attribute__((unused)) = (kaapi_metadata_info_t*)arg1;
  kaapi_address_space_id_t asid  __attribute__((unused)) = (kaapi_address_space_id_t)arg2;
  KAAPI_ATOMIC_DECR(&grp->counter);
}

/*
*/
int kaapi_memory_sync_data(kaapi_memgroup_t* grp, void* ptr)
{
  uint16_t lid0 = kaapi_memory_asid_get_lid( kaapi_local_asid );
  kaapi_assert_debug(lid0 < KAAPI_MEMORY_MAX_NODES);
  kaapi_access_t a;
  kaapi_access_init(&a, ptr);

  kaapi_metadata_info_t* mdi = kaapi_dsm_findaccess_on_node(
      &kaapi_the_dsm,
      kaapi_local_asid,
      0, /* do not create if not existing */
      &a,
      0
  );
  if (mdi ==0)
    return EINVAL;

  if (kaapi_memory_replica_is_valid(mdi, lid0))
    return 0;

  KAAPI_ATOMIC_INCR(&grp->counter);
  int err = kaapi_dsm_fetch_on(
      &kaapi_the_dsm,
      kaapi_local_asid,
      mdi,
      callback_signal_grp_on_receive_cbk, (void*)grp, (void*)mdi, (void*)kaapi_local_asid
  );
  if (err != EINPROGRESS)
    KAAPI_ATOMIC_DECR(&grp->counter);
  return err;
}

/*
*/
int kaapi_memory_group_wait( kaapi_memgroup_t* grp )
{
  useconds_t d= 1;
  while (1)
  {
    int counter = 1000;
    while (--counter)
    {
      if (KAAPI_ATOMIC_READ(&grp->counter) == 0) return 0;
      usleep(d);
    }
    d *= 2;
    if (d > 1000) d = 1000;
  }

#if 0
  kaapi_assert(0 == pthread_mutex_lock(&grp->mutex));
  grp->iswaiting = 1;
  while (KAAPI_ATOMIC_READ(&grp->counter) != 0)
    kaapi_assert(0==pthread_cond_wait(&grp->cond, &grp->mutex));
  grp->iswaiting = 0;
  kaapi_assert(0 == pthread_mutex_unlock(&grp->mutex));
#endif
  return 0;
}

/*
*/
int kaapi_memory_group_destroy( kaapi_memgroup_t* grp )
{
#if 0
  kaapi_assert(0 == pthread_mutex_destroy(&grp->mutex));
  kaapi_assert(0 == pthread_cond_destroy(&grp->cond));
#endif
  return 0;
}
