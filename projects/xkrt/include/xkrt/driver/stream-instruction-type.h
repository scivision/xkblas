/* ************************************************************************** */
/*                                                                            */
/*   stream-instruction-type.h                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:00:48 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __STREAM_INSTRUCTION_TYPE_H__
# define __STREAM_INSTRUCTION_TYPE_H__

typedef enum    xkrt_stream_instruction_type_t
{
    XKRT_STREAM_INSTR_TYPE_KERN         = 0,

    XKRT_STREAM_INSTR_TYPE_COPY_H2H_1D  = 1,
    XKRT_STREAM_INSTR_TYPE_COPY_H2D_1D  = 2,
    XKRT_STREAM_INSTR_TYPE_COPY_D2H_1D  = 3,
    XKRT_STREAM_INSTR_TYPE_COPY_D2D_1D  = 4,

    XKRT_STREAM_INSTR_TYPE_COPY_H2H_2D  = 5,
    XKRT_STREAM_INSTR_TYPE_COPY_H2D_2D  = 6,
    XKRT_STREAM_INSTR_TYPE_COPY_D2H_2D  = 7,
    XKRT_STREAM_INSTR_TYPE_COPY_D2D_2D  = 8,

    XKRT_STREAM_INSTR_TYPE_MAX          = 9
}               xkrt_stream_instruction_type_t;

const char * xkrt_stream_instruction_type_to_str(xkrt_stream_instruction_type_t type);

#endif /* __STREAM_INSTRUCTION_H__ */
