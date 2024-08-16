# if !defined(CACHE_LINE_SIZE)
#  if __cpp_lib_hardware_interference_size >= 201603
#   include <new>
#   define CACHE_LINE_SIZE std::hardware_constructive_interference_size
#  else
#   define CACHE_LINE_SIZE 64
#  endif
# endif
