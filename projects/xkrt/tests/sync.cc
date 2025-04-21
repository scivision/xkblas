# include <xkrt/xkrt.h>
# include <assert.h>

int
main(void)
{
    xkrt_runtime_t runtime;
    assert(xkrt_init(&runtime) == 0);
    assert(xkrt_sync(&runtime) == 0);
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
