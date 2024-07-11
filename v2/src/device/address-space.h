#ifndef __ADDRESS_SPACE_H__
# define __ADDRESS_SPACE_H__

/* ============================ Address space ================================= */

#define XKBLAS_PROC_TYPE_DEFAULT  0
#define XKBLAS_PROC_TYPE_HOST     0
#define XKBLAS_PROC_TYPE_CUDA     1
#define XKBLAS_PROC_TYPE_HIP      2
#define XKBLAS_PROC_TYPE_INTERNAL 3
#define XKBLAS_PROC_TYPE_MAX      4
#define XKBLAS_PROC_TYPE_CPU      XKBLAS_PROC_TYPE_HOST
#define XKBLAS_PROC_TYPE_GPU      XKBLAS_PROC_TYPE_CUDA

/* Representation of address space id over 64bits integer
   - 16bits: local identifier
   - 32bits: ?
   - 12bits: reserved
   - 4bits : 16 values for coding architecture
   On a multicore, each address space has a local identifier. The global identifier
   was used to identified process in the same way as MPI rank.
*/
#define XKBLAS_ASID_MASK_LID       0x000000000000FFFFULL /* size = 16 */
#define XKBLAS_ASID_SHIFT_LID      0UL                   /* shift = 0 */
#define XKBLAS_ASID_MASK_GID       0x0000FFFFFFFF0000ULL /* size = 32 */
#define XKBLAS_ASID_SHIFT_GID      16UL                  /* shift = 16 */
#define XKBLAS_ASID_MASK_RESERVED  0x0FFF000000000000ULL /* size = 12 */
#define XKBLAS_ASID_MASK_ARCH      0xF000000000000000ULL /* size = 4 */
#define XKBLAS_ASID_SHIFT_ARCH     60UL                  /* shift = 0 */

typedef uint32_t    xkblas_globalid_t;
typedef uint64_t    xkblas_address_space_id_t;

static inline xkblas_address_space_id_t xkblas_memory_create_asid(
        xkblas_globalid_t gid,
        uint16_t lid,
        uint8_t arch
        )
{
    xkblas_address_space_id_t asid = (((uint64_t)lid) & XKBLAS_ASID_MASK_LID)
        | ((((uint64_t)gid) << XKBLAS_ASID_SHIFT_GID) & XKBLAS_ASID_MASK_GID)
        | ((((uint64_t)arch) << XKBLAS_ASID_SHIFT_ARCH) & XKBLAS_ASID_MASK_ARCH);
    return asid;
}

static inline xkblas_globalid_t xkblas_memory_get_current_globalid(void)
{ return 0;}

/** Return the gid of the address space
  \param kasid [IN] an address space identifier
  \return the gid encoded into the address space identifier
  */
static inline xkblas_globalid_t xkblas_memory_asid_get_gid( xkblas_address_space_id_t kasid )
{ return (xkblas_globalid_t)((kasid & XKBLAS_ASID_MASK_GID)>> XKBLAS_ASID_SHIFT_GID); }

/** Return the arch type of the address space location.
  \param kasid [IN] an address space identifier
  \return the type encoded into the address space identifier
  */
static inline uint8_t xkblas_memory_asid_get_arch( xkblas_address_space_id_t kasid )
{ return (uint8_t)((kasid & XKBLAS_ASID_MASK_ARCH) >> XKBLAS_ASID_SHIFT_ARCH); }

/** Return the local identifier of the address space location.
  \param kasid [IN] an address space identifier
  \return the local id encoded into the address space identifier
  */
static inline uint16_t xkblas_memory_asid_get_lid( xkblas_address_space_id_t kasid )
{ return (uint16_t)((kasid & XKBLAS_ASID_MASK_LID) >> XKBLAS_ASID_SHIFT_LID); }

#endif /* __ADDRESS_SPACE_H__ */
