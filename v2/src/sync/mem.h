#ifndef __MEM_H__
# define __MEM_H__

#if defined(__i386__)|| defined(__x86_64)
#  if !defined(__MIC__)
#      define mem_pause() do { __asm__ __volatile__("pause\n\t"); } while (0)
#  else
#    define mem_pause() do { __asm__ __volatile__("mov $20, %rax; delay %rax\n\t"); __asm__ __volatile__ ("":::"memory"); } while (0)
#  endif
#else
#  define mem_pause()
#endif

static inline void mem_barrier(void)
{
#if defined(__x86_64) || defined(__i386__)
#  if !defined(__MIC__)
  __asm__ __volatile__ ("mfence":::"memory");
#  endif
#else
  OSMemoryBarrier();
#endif
}

#endif /* __MEM_H__ */
