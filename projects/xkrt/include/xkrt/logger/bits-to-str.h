/* ************************************************************************** */
/*                                                                            */
/*   bits-to-str.h                                                .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/05 16:28:42 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:01:01 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# ifndef __XKRT_BITS_TO_STR_H__
#  define __XKRT_BITS_TO_STR_H__

extern "C" {
    void xkrt_bits_to_str(char * buffer, unsigned char * mem, size_t nbytes);
};

# endif /* __XKRT_BITS_TO_STR_H__ */
