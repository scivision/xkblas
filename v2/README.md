# PERFORMANCE IDEA
If memory capacity is not a limitation, add public interfaces to preallocate memory onto the device
```C

host_view_t host_view = { host_ptr, LD, M, N, sizeof_type };

size_t size = M * N * sizeof_type;
void * ptr = xkblas_memory_allocate(device, size);
device_view_t deview_view = { ptr, LD, M, N, sizeof_type };

xkblas_memory_register_replicate(host_view, device, device_view);
```
So a replicate continuous in memory is allocated all at once on the device

# PERFORMANCE IDEA 2
Merge fetch operation if they are continuous in memory

# PERFORMANCE IDEA 3
Memory coherency currently insert 1 big task with all the region; meaning no data will start moving until the entire region had been computed (even though some parts may be ready early)

# LICENSE
- Set header in all files, a CECILL

# Impacts on previous applications
- users must explicitly call `xkblas_thread_init` on any thread before making any other calls to xkblas on that thread

# To improve
- Tasks descriptor is allocated in the producer thread memory... while it will be heavily accessed and modified by consumers
- Tasks are currently never deleted, as they may be referenced in the memory tree
