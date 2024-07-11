#ifndef __MEMORY_H__
# define __MEMORY_H__

# include "device/io.h"
# include "sync/mutex.h"

///////////////////////////
// Driver devices memory //
///////////////////////////
typedef uint64_t xkblas_address_space_id_t;

# pragma message(TODO "What is a 'xkblas_alloc_data' struct ?")
struct xkblas_alloc_data;
typedef struct xkblas_alloc_data xkblas_alloc_data_t;

# pragma message(TODO "What is a 'xkblas_alloc_chunk' struct ?")
struct xkblas_alloc_chunk;
typedef struct xkblas_alloc_chunk xkblas_alloc_chunk_t;

/** Type of pointer for all address spaces.
    The pointer encode both the pointer (field ptr) and the location of the address space
    in asid. Pointer arithmetic is allowed on this type on the ptr field.
    If pointer is on device with disjoint adress space, meta is a host data to help storing
    meta data.
*/
typedef struct  xkblas_pointer_t
{
    xkblas_address_space_id_t asid;
    uintptr_t                ptr;
    uintptr_t                meta;
}               xkblas_pointer_t;


/** Type of allowed memory view for the memory interface:
    - 1D array (base, size)
      simple contiguous 1D array
    - 2D array (base, size[2], lda)
      assume a row major storage of the memory : the 2D array has
      size[0] rows of size[1] rowwidth. lda is used to pass from
      one row to the next one. 
    The base (xkblas_pointer_t) is not part of the view description
*/
#define XKBLAS_MEMORY_VIEW_1D 1 
#define XKBLAS_MEMORY_VIEW_2D 2  /* assume row major */
#define XKBLAS_MEMORY_VIEW_3D 3
#define XKBLAS_MEMORY_STORAGE_ROWMAJOR 1
#define XKBLAS_MEMORY_STORAGE_COLMAJOR 2
typedef struct xkblas_memory_view_t {
  uintptr_t     offset;
  size_t        size[3];
  size_t        ld;
  size_t        wordsize;
  uint8_t       type;
  uint8_t       storage;
} xkblas_memory_view_t;


# define XKBLAS_MEMORY_VALUE_TYPE uint16_t

# define XKBLAS_MEMORY_DEVICE_FLAG_NONE          0
# define XKBLAS_MEMORY_DEVICE_FLAG_MOSTLY_FULL   0x1
# define XKBLAS_MEMORY_DEVICE_FLAG_FULL          0x2

# pragma message(TODO "Make 'xkblas_device_memory_t' an abstract C++ class to be implemented by drivers")

typedef struct  xkblas_device_memory_t
{
    xkblas_address_space_id_t asid;
    // xkblas_device_t * device;
    xkblas_mutex_t mem_lock;
    xkblas_alloc_data_t * freelist_bloc;
    xkblas_alloc_data_t * freelist_metabloc;
    xkblas_alloc_chunk_t * free_chunk_list;
    xkblas_alloc_chunk_t * main_chunk;

    /* Virtualization of alloc/free on the offload memory device */
    uintptr_t (*f_alloc)(int device_id,  size_t size, int * flag);
    void  (*f_free)(int device_id, uintptr_t ptr, size_t size);

    /* returns:
       0: success
       EINPROGRESS : pending operations on the device
       else error
    */
    int   (*f_copy)(struct xkblas_device_memory_t*,
                    xkblas_pointer_t /* dest*/,
                    const xkblas_memory_view_t* /*view_dest*/,
                    xkblas_pointer_t /*src*/,
                    const xkblas_memory_view_t* /*view_src*/,
                    int flags, /* 0, 1, 2 */
                    xkblas_io_callback_func_t cbk,
                    void* arg0, void* arg1, void* arg2
    );
    int  (*f_memsync)(struct xkblas_device_memory_t*, int begend);

    /* to help to manage cache */
    size_t (*f_get_mem_info)(struct xkblas_device_memory_t*, size_t*, size_t*);
    size_t (*f_get_free_mem)(struct xkblas_device_memory_t*);

    /* return source lid to reach lid_dest knowing valid_bit and xfer_bit for the data */
    uint16_t (*f_get_source)( struct xkblas_device_memory_t*, uint16_t, XKBLAS_MEMORY_VALUE_TYPE, XKBLAS_MEMORY_VALUE_TYPE );

}               xkblas_device_memory_t;

#endif /* __MEMORY_H__ */
