/* ************************************************************************** */
/*                                                                            */
/*   color.h                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __COLOR_H__
# define __COLOR_H__

typedef enum    Color
{
    BLACK   = 0,
    RED     = 1
}               Color;

extern const char * COLORS[];

#endif /* __COLOR_H__ */
