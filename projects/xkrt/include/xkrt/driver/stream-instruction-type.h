/* ************************************************************************** */
/*                                                                            */
/*   stream-instruction-type.h                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:54:09 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
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
