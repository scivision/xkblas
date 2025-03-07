/* ************************************************************************** */
/*                                                                            */
/*   bits-to-str.h                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/18 15:08:36 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/07 17:07:51 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# ifndef __XKRT_BITS_TO_STR_H__
#  define __XKRT_BITS_TO_STR_H__

extern "C" {
    void xkrt_bits_to_str(char * buffer, unsigned char * mem, size_t nbytes);
};

# endif /* __XKRT_BITS_TO_STR_H__ */
