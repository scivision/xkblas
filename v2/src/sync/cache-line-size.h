# if !defined(CACHE_LINE_SIZE)
#  pragma message("Assuming CACHE_LINE_SIZE = 64 bytes, use -DCACHE_LINE_SIZE if wrong. Consider using 'std::hardware_destructive_interference_size' instead")
#  define CACHE_LINE_SIZE 64
# endif
