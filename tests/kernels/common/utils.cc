/* ************************************************************************** */
/*                                                                            */
/*   utils.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

static inline char
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
