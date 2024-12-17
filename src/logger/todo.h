/* ************************************************************************** */
/*                                                                            */
/*   todo.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
