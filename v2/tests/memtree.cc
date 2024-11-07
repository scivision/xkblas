# include "device/memory-tree.hpp"

int
main(void)
{
    KMemoryAccess<2> accesses[] = {
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800020, 8, 0, 0, 4, 4, 4, ACCESS_MODE_R),        // 1
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800120, 8, 0, 0, 4, 4, 4, ACCESS_MODE_R),        // 2
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800420, 8, 0, 0, 4, 4, 4, ACCESS_MODE_RW),       // 3
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800020, 8, 0, 0, 4, 4, 4, ACCESS_MODE_R),        // 4
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800120, 8, 0, 4, 4, 4, 4, ACCESS_MODE_R),        // 5
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800420, 8, 0, 4, 4, 4, 4, ACCESS_MODE_RW),       // 6
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800020, 8, 0, 4, 4, 4, 4, ACCESS_MODE_R),        // 7
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800120, 8, 4, 0, 4, 4, 4, ACCESS_MODE_R),        // 8
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800420, 8, 0, 0, 4, 4, 4, ACCESS_MODE_RW),       // 9
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800020, 8, 0, 4, 4, 4, 4, ACCESS_MODE_R),        // 10
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800120, 8, 4, 4, 4, 4, 4, ACCESS_MODE_R),        // 11
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800420, 8, 0, 4, 4, 4, 4, ACCESS_MODE_RW),       // 12
        // FUCK 0x7f3d6d8004a0 - using chunk 0x7f3d6e000200 - KMemoryAccess Interval is (4371931463717, 4371931463721) x (0, 16)
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800020, 8, 4, 0, 4, 4, 4, ACCESS_MODE_R),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800120, 8, 0, 4, 4, 4, 4, ACCESS_MODE_R),
        // FUCK 0x7f3d6d8001a0 - using chunk 0x7f3d6e000280 - KMemoryAccess Interval is (4371931463693, 4371931463697) x (0, 16)
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800420, 8, 4, 4, 4, 4, 4, ACCESS_MODE_RW),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800020, 8, 4, 0, 4, 4, 4, ACCESS_MODE_R),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800120, 8, 0, 0, 4, 4, 4, ACCESS_MODE_R),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800420, 8, 4, 0, 4, 4, 4, ACCESS_MODE_RW),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800020, 8, 4, 4, 4, 4, 4, ACCESS_MODE_R),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800120, 8, 4, 4, 4, 4, 4, ACCESS_MODE_R),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800420, 8, 4, 4, 4, 4, 4, ACCESS_MODE_RW),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800020, 8, 4, 4, 4, 4, 4, ACCESS_MODE_R),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800120, 8, 4, 0, 4, 4, 4, ACCESS_MODE_R),
        KMemoryAccess<2>(MATRIX_COLMAJOR, (void *) 0x7f3d6d800420, 8, 4, 0, 4, 4, 4, ACCESS_MODE_RW),
    };

    const size_t ld = 131072;
    KMemoryTree<2> tree(ld);

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
