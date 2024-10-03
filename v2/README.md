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

# LICENSE
- Set header in all files, a CECILL

# To improve
- If OCR is set on a successor task, when the predecessor writter completes
  - the successor device is known: set it already
  - if reader predecessor completes and the device is known, transfer can be initiated without waiting for all predecessors to complete
- Tasks descriptor is allocated in the producer thread memory... while it will be heavily accessed and modified by consumers
- Tasks are currently never deleted (?)
- Merge continuous memory block to a single transfer
- memory ordering on atomic
- Task descriptor size
- xkblas (and cuda) assume col major accesses... can we manage row major too ?
- task descriptor allocation - currently spamming 'new' and 'malloc' - use the ThreadWorker / ThreadProducer stack allocation() and deallocate-all() instead

# Question for XKBLAS/v1
- Why is it not deadlocking when stream queues are full ???

# Remark for XKBLAS/V1
- Device thread 'requests' mechanism got removed - use tasks instead (as per memory-coherent-async tasks)

# DARK-AREA - I DONT UNDERSTAND WHY
- Having LIFO ThreadWorker queue makes computation incorrect (against FIFO that is correct!)
- Inverting 2-interval-btree coordinates (and associated memory-tree.hpp usage) makes computation incorrect
