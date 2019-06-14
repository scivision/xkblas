/**
 *
 * @file testing_zauxiliary.c
 *
 * @copyright 2009-2014 The University of Tennessee and The University of
 *                      Tennessee Research Foundation. All rights reserved.
 * @copyright 2012-2018 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
 *                      Univ. Bordeaux. All rights reserved.
 *
 ***
 *
 * @brief Chameleon CHAMELEON_Complex64_t auxiliary testings routines
 *
 * @version 1.0.0
 * @author Mathieu Faverge
 * @author Cédric Castagnède
 * @author Thierry Gautier: adaptation to xkblas
 * @date 2010-11-15
 * @precisions normal z -> c d s
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined( _WIN32 ) || defined( _WIN64 )
#include <windows.h>
#else  /* Non-Windows */
#include <unistd.h>
#include <sys/resource.h>
#endif
#include "xkblas.h"
#include "testing_zauxiliary.h"

int   IONE     = 1;
int   ISEED[5] = {0,0,0,1,0};   /* initial seed for zlarnv() */

int     side[2]   = { CblasLeft,    CblasRight };
int     uplo[2]   = { CblasUpper,   CblasLower };
int     diag[2]   = { CblasNonUnit, CblasUnit  };
int    trans[3]  = { CblasNoTrans, CblasTrans, CblasConjTrans };

char *sidestr[2]  = { "Left ", "Right" };
char *uplostr[2]  = { "Upper", "Lower" };
char *diagstr[2]  = { "NonUnit", "Unit   " };
char *transstr[3] = { "N", "T", "H" };


int main (int argc, char **argv)
{
    int ncores, ngpus, nb, ib;
    int info = 0;
    char func[32];

    /* Check for number of arguments*/
    if ( argc < 6 ) {
        printf(" Proper Usage is : ./ztesting ncores ngpus nb ib FUNC ...\n"
               "   - ncores : number of cores\n"
               "   - ngpus  : number of GPUs\n"
               "   - nb     : define the tile size\n"
               "   - ib     : define the inner tile size\n"
               "   - FUNC   : name of function to test\n"
               "              name could be GEMM TRMM TRSM SYMM SYRK SR2K"
#if defined(PRECISION_z) || defined(PRECISION_c)
               " HEMM HERK HER2K\n"
#else
               "\n"
#endif

               "   - ... plus arguments depending on the testing function \n");
        exit(1);
    }

    sscanf( argv[1], "%d",   &ncores );
    sscanf( argv[2], "%d",   &ngpus  );
    sscanf( argv[3], "%d",   &nb     );
    sscanf( argv[4], "%d",   &ib     );
    sscanf( argv[5], "%31s",  func   );

    argc -= 6;
    argv += 6;
    info  = 0;

    xkblas_set_param( nb, sizeof(Complex64_t) );

    /* Initialize Kaapi */
    xkblas_init();


    /*
     * Blas Level 3
     */
    if ( strcmp(func, "GEMM") == 0 ) {
        info += testing_zgemm( argc, argv );
    }
    else if ( strcmp(func, "TRMM") == 0 ) {
        info += testing_ztrmm( argc, argv );
    }
    else if ( strcmp(func, "TRSM") == 0 ) {
        info += testing_ztrsm( argc, argv );
    }
    else if ( strcmp(func, "SYMM") == 0 ) {
        info += testing_zsymm( argc, argv );
    }
    else if ( strcmp(func, "SYRK") == 0 ) {
        info += testing_zsyrk( argc, argv );
    }
    else if ( strcmp(func, "SYR2K") == 0 ) {
        info += testing_zsyr2k( argc, argv );
    }
#if defined(PRECISION_z) || defined(PRECISION_c)
    else if ( strcmp(func, "HEMM") == 0 ) {
        info += testing_zhemm( argc, argv );
    }
    else if ( strcmp(func, "HERK") == 0 ) {
        info += testing_zherk( argc, argv );
    }
    else if ( strcmp(func, "HER2K") == 0 ) {
        info += testing_zher2k( argc, argv );
    }
#endif
    else
      abort();

    if ( info == -1 ) {
        printf( "TESTING %s FAILED : incorrect number of arguments\n", func);
    } else if ( info == -2 ) {
        printf( "TESTING %s FAILED : not enough memory\n", func);
    }

    xkblas_finalize();

    return info;
}
