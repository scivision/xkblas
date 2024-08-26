# LICENSE
- Set header in all files, a CECILL

# Impacts on previous applications
- users must explicitly call `xkblas_thread_init` on any thread before making any other calls to xkblas on that thread

# To improve
- Tasks descriptor is allocated in the producer thread memory... while it will be heavily accessed and modified by consumers
- Tasks are currently never deleted, as they may be referenced in the memory tree
