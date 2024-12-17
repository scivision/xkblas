#ifndef __TODO_H__
# define __TODO_H__

// usage: # pragma message(TODO "Rename 'cuda' conf variables to something vendor-agnostic")

# define Stringize( L )     #L
# define MakeString( M, L ) M(L)
# define $Line MakeString( Stringize, __LINE__ )
# define TODO __FILE__ "(" $Line ") : TODO: "

#endif
