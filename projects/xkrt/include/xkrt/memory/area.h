/* ************************************************************************** */
/*                                                                            */
/*   area.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/22 04:12:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __AREA_H__
# define __AREA_H__

# include <xkrt/sync/mutex.h>

///////////////////////////
// Driver devices memory //
///////////////////////////

typedef enum    xkrt_area_chunk_state_t
{
    XKRT_ALLOC_CHUNK_STATE_FREE       = 0,
    XKRT_ALLOC_CHUNK_STATE_ALLOCATED  = 1,

}               xkrt_area_chunk_state_t;

/**
 * Represent a segment of memory in device memory (used by custom allocator)
 * It is placed in two chained list:
 *  - the list of all chunk in device memory
 *  - the list of free chunk in device memory
*/
typedef struct  xkrt_area_chunk_t
{
    uintptr_t ptr;                          /* position of memory in device */
    size_t size;                            /* size of the segment in byte */
    int state;                              /* state of the chunk */
    struct xkrt_area_chunk_t * prev;        /* previous chunk in double chained list */
    struct xkrt_area_chunk_t * next;        /* next chunk in double chained list */
    struct xkrt_area_chunk_t * freelink;    /* next freechunk in the chained list */
    int use_counter;                        /* used in the memory-tree to count how many blocks relies on that allocation chunk */
    int area_idx;                           /* memory area index in the device (TODO: bad design) */
}               xkrt_area_chunk_t;

/* The device memory with allocation information */
typedef struct  xkrt_area_t
{
    xkrt_mutex_t lock;
    xkrt_area_chunk_t chunk0;
    xkrt_area_chunk_t * free_chunk_list;

}               xkrt_area_t;

#endif /* __AREA_H__ */
