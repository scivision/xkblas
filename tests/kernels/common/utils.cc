static inline const char
cblas2blas_op(int trans)
{
    switch (trans)
    {
        case CblasNoTrans:
            return 'N';
        case CblasTrans:
           return 'T';
        case CblasConjTrans:
           return 'C';
    }
    abort();
}
