/* ************************************************************************** */
/*                                                                            */
/*   device-memory.h                                                          */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 12:00:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEVICE_MEMORY_H__
# define __DEVICE_MEMORY_H__

# include <kaapi/sync/mutex.h>

///////////////////////////
// Driver devices memory //
///////////////////////////

typedef enum    kaapi_alloc_chunk_state_t
{
    KAAPI_ALLOC_CHUNK_STATE_FREE       = 0,
    KAAPI_ALLOC_CHUNK_STATE_ALLOCATED  = 1,

}               kaapi_alloc_chunk_state_t;

/**
 * Represent a segment of memory in device memory (used by custom allocator)
 * It is placed in two chained list:
 *  - the list of all chunk in device memory
 *  - the list of free chunk in device memory
*/
typedef struct  kaapi_alloc_chunk_t
{
    uintptr_t device_ptr;                   /* position of memory in device */
    size_t size;                            /* size of the segment in byte */
    int state;                              /* state of the chunk */
    struct kaapi_alloc_chunk_t * prev;     /* previous chunk in double chained list */
    struct kaapi_alloc_chunk_t * next;     /* next chunk in double chained list */
    struct kaapi_alloc_chunk_t * freelink; /* next freechunk in the chained list */
    int use_counter;                        /* used in the memory-tree to count how many blocks relies on that allocation chunk */
}               kaapi_alloc_chunk_t;

/* The device memory with allocation information */
typedef struct  kaapi_device_memory_t
{
    kaapi_mutex_t lock;
    kaapi_alloc_chunk_t chunk0;
    kaapi_alloc_chunk_t * free_chunk_list;

}               kaapi_device_memory_t;

#endif /* __DEVICE_MEMORY_H__ */
