# include <xkrt/xkrt.h>
# include <assert.h>

int
main(void)
{
    assert(xkrt_init() == 0);
    assert(xkrt_sync() == 0);
    assert(xkrt_deinit() == 0);

    return 0;
}
