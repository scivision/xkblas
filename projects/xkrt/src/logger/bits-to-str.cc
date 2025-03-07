/* ************************************************************************** */
/*                                                                            */
/*   bits-to-str.cc                                                           */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/07 16:57:59 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/07 17:42:27 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
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
