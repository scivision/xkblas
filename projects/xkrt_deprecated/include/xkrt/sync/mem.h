/* ************************************************************************** */
/*                                                                            */
/*   mem.h                                                        .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/11 15:12:55 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:06:23 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __MEM_H__
# define __MEM_H__

/**
 *  Retrieved from xkrt_atomic.h
 */

#if defined(__i386__)|| defined(__x86_64)
#  if !defined(__MIC__)
#      define mem_pause() do { __asm__ __volatile__("pause\n\t"); } while (0)
#  else
#    define mem_pause() do { __asm__ __volatile__("mov $20, %rax; delay %rax\n\t"); __asm__ __volatile__ ("":::"memory"); } while (0)
#  endif
#else
#  define mem_pause()
#endif

#if defined(__APPLE__)
#  include <libkern/OSAtomic.h>
static inline void writemem_barrier()
{
#  if defined(__x86_64) || defined(__i386__)
  /* not need sfence on X86 archi: write are ordered __asm__ __volatile__ ("sfence":::"memory"); */
  __asm__ __volatile__ ("":::"memory");
#  else
  OSMemoryBarrier();
#  endif
}

static inline void readmem_barrier()
{
#  if defined(__x86_64) || defined(__i386__)
  __asm__ __volatile__ ("":::"memory");
//  __asm__ __volatile__ ("lfence":::"memory");
#  else
  OSMemoryBarrier();
#  endif
}

/* should be both read & write barrier */
static inline void mem_barrier()
{
#  if defined(__x86_64) || defined(__i386__)
  /** Mac OS 10.6.8 with gcc 4.2.1 has a buggy __sync_synchronize();
      gcc-4.4.4 pass the test with sync_synchronize
  */
#if !defined(__MIC__)
  __asm__ __volatile__ ("mfence":::"memory");
#endif
#  else
  OSMemoryBarrier();
#  endif
}

#elif defined(__linux__)    /* defined(__APPLE__) */
static inline void writemem_barrier()
{
#  if defined(__x86_64) || defined(__i386__)
  /* not need sfence on X86 archi: write are ordered */
  __asm__ __volatile__ ("":::"memory");
#  elif defined(__GNUC__)
  __sync_synchronize();
#  else
#  error "Compiler not supported"
/* xlC ->__lwsync() / bultin */
#  endif
}

static inline void readmem_barrier()
{
#  if defined(__x86_64) || defined(__i386__)
  /* not need lfence on X86 archi: read are ordered */
  __asm__ __volatile__ ("":::"memory");
#  elif defined(__GNUC__)
  __sync_synchronize();
#  else
#  error "Compiler not supported"
/* xlC ->__lwsync() / bultin */
#  endif
}

/* should be both read & write barrier */
static inline void mem_barrier()
{
#  if defined(__GNUC__) || defined(__ICC)
  __sync_synchronize();
#  elif defined(__x86_64) || defined(__i386__)
  __asm__ __volatile__ ("mfence":::"memory");
#elif defined(__arm__) || defined(__aarch64__)
  __asm__ __volatile__ ("dmb ish":::"memory");
#  else
#   error "Compiler not supported"
/* bultin ?? */
#  endif
}

#elif defined(_WIN32)
static inline void writemem_barrier()
{
  /* Compiler fence to keep operations from */
  /* not need sfence on X86 archi: write are ordered */
  __asm__ __volatile__ ("":::"memory");
}

static inline void readmem_barrier()
{
  /* Compiler fence to keep operations from */
  /* not need lfence on X86 archi: read are ordered */
  __asm__ __volatile__ ("":::"memory");
}

#else
#  error "Undefined barrier"
#endif

#endif /* __MEM_H__ */
