/* ************************************************************************** */
/*                                                                            */
/*   bits-to-str.cc                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/26 19:40:36 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 17:56:40 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <assert.h>
# include <stdio.h>

extern "C"
void
xkrt_bits_to_str(char * buffer, unsigned char * mem, size_t nbytes)
{
    buffer[8*nbytes] = 0;
    size_t k = 8*nbytes - 1;
    for (int i = (int)nbytes - 1 ; i >= 0 ; --i)
        for (int j = 0 ; j < 8 ; ++j)
            buffer[k--] = (mem[i] & (1 << j)) ? '1' : '0';
}
