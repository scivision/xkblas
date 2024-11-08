# include "device/memory-tree.hpp"

int
main(void)
{
    KMemoryAccess<2> accesses[] = {
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 3648, 3648, 352, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 3648, 3648, 352, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 0, 352, 456, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 456, 352, 456, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 3648, 3648, 352, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 4104, 352, 456, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 3648, 3648, 352, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 3192, 352, 456, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 3648, 3648, 352, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 4560, 352, 440, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 3648, 3648, 352, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 3648, 352, 456, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 3648, 3648, 352, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 2736, 352, 456, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 0, 3648, 456, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 0, 352, 456, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 0, 0, 456, 456, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 0, 3648, 456, 352, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 3648, 4560, 352, 440, 4, ACCESS_MODE_R),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa315f62654, 5000, 0, 4560, 456, 440, 4, ACCESS_MODE_RW),
KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7fa310004554, 5000, 456, 3648, 456, 352, 4, ACCESS_MODE_R)
    };

    const size_t ld = 131072;
    KMemoryTree<2> tree(ld, 1);

    KMemoryTreeNodeSearch<2> search(0);

    for (unsigned int i = 0 ; i < sizeof(accesses) / sizeof(KMemoryAccess<2>) ; ++i)
    {
        KMemoryAccess<2> * access = accesses + i;

        search.prepare_insert(access);
        tree.insert(search, access->cubes[0], access->mode);
        tree.insert(search, access->cubes[1], access->mode);
    }

    return 0;
}
