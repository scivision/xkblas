#ifndef __MIN_MAX_H__
# define __MIN_MAX_H__

# ifndef MIN
#  define MIN(X, Y) ((Y) < (X) ? (Y) : (X))
# endif /* MIN */

# ifndef MAX
#  define MAX(X, Y) ((X) < (Y) ? (Y) : (X))
# endif /* MAX */

#endif
