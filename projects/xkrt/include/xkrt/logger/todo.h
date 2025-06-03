/* ************************************************************************** */
/*                                                                            */
/*   todo.h                                                       .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/10 10:59:00 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:02:09 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __TODO_H__
# define __TODO_H__

// usage: # pragma message(TODO "Rename 'cuda' conf variables to something vendor-agnostic")

# define Stringize( L )     #L
# define MakeString( M, L ) M(L)
# define $Line MakeString( Stringize, __LINE__ )
# define TODO __FILE__ "(" $Line ") : TODO: "

#endif
