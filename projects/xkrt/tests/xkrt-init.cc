# include <xkrt/xkrt.h>
# include <assert.h>

int
main(void)
{
    xkrt_runtime_t runtime;
    assert(xkrt_init(&runtime) == 0);
    xkrt_deinit(&runtime);
    return 0;
}
