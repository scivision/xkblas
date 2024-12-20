# include <kaapi/kaapi.h>
# include <assert.h>

int
main(void)
{
    assert(kaapi_init() == 0);
    return 0;
}
