__kernel
void
sleep_kernel(ulong iterations, int task_id)
{
    size_t gid = get_global_id(0);
    if (gid == 0)
    {
        if (iterations == 123456)
            printf("task %d starts\n", task_id);

        ulong counter = 0;
        for (ulong i = 0; i < iterations; i++) {
            counter += i % 3;  // Dummy computation to prevent the loop from being optimized away
        }

        if (iterations == 123456)
            printf("task %d completed for %lu iterations - with a counter of %lu\n", task_id, iterations, counter);
    }
}
