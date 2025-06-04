/* ************************************************************************** */
/*                                                                            */
/*   allocator.h                                                  .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/18 22:56:54 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:37:16 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __ALLOCATOR_H__
#  define __ALLOCATOR_H__

#  include <stdlib.h>

void   xkbm_mem_touch(void * ptr, size_t size);
void * xkbm_alloc_and_touch(const size_t size);
void * xkbm_mem_alloc(size_t size);
void   xkbm_free(void * ptr, size_t size);

# endif /* __ALLOCATOR_H__ */
