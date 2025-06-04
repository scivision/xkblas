/* ************************************************************************** */
/*                                                                            */
/*   logger-clblast.h                                             .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/02/15 00:45:32 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:01:11 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                                */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOGGER_CLBLAST_H__
# define __LOGGER_CLBLAST_H__

# include <xkrt/logger/logger.h>
# include <clblast_c.h>

static const char *
clblast_error_to_str(CLBlastStatusCode status)
{
    switch (status)
    {
        case CLBlastSuccess                   : return "CLBlastSuccess";
        case CLBlastOpenCLCompilerNotAvailable: return "CLBlastOpenCLCompilerNotAvailable";
        case CLBlastTempBufferAllocFailure    : return "CLBlastTempBufferAllocFailure";
        case CLBlastOpenCLOutOfResources      : return "CLBlastOpenCLOutOfResources";
        case CLBlastOpenCLOutOfHostMemory     : return "CLBlastOpenCLOutOfHostMemory";
        case CLBlastOpenCLBuildProgramFailure : return "CLBlastOpenCLBuildProgramFailure";
        case CLBlastInvalidValue              : return "CLBlastInvalidValue";
        case CLBlastInvalidCommandQueue       : return "CLBlastInvalidCommandQueue";
        case CLBlastInvalidMemObject          : return "CLBlastInvalidMemObject";
        case CLBlastInvalidBinary             : return "CLBlastInvalidBinary";
        case CLBlastInvalidBuildOptions       : return "CLBlastInvalidBuildOptions";
        case CLBlastInvalidProgram            : return "CLBlastInvalidProgram";
        case CLBlastInvalidProgramExecutable  : return "CLBlastInvalidProgramExecutable";
        case CLBlastInvalidKernelName         : return "CLBlastInvalidKernelName";
        case CLBlastInvalidKernelDefinition   : return "CLBlastInvalidKernelDefinition";
        case CLBlastInvalidKernel             : return "CLBlastInvalidKernel";
        case CLBlastInvalidArgIndex           : return "CLBlastInvalidArgIndex";
        case CLBlastInvalidArgValue           : return "CLBlastInvalidArgValue";
        case CLBlastInvalidArgSize            : return "CLBlastInvalidArgSize";
        case CLBlastInvalidKernelArgs         : return "CLBlastInvalidKernelArgs";
        case CLBlastInvalidLocalNumDimensions : return "CLBlastInvalidLocalNumDimensions";
        case CLBlastInvalidLocalThreadsTotal  : return "CLBlastInvalidLocalThreadsTotal";
        case CLBlastInvalidLocalThreadsDim    : return "CLBlastInvalidLocalThreadsDim";
        case CLBlastInvalidGlobalOffset       : return "CLBlastInvalidGlobalOffset";
        case CLBlastInvalidEventWaitList      : return "CLBlastInvalidEventWaitList";
        case CLBlastInvalidEvent              : return "CLBlastInvalidEvent";
        case CLBlastInvalidOperation          : return "CLBlastInvalidOperation";
        case CLBlastInvalidBufferSize         : return "CLBlastInvalidBufferSize";
        case CLBlastInvalidGlobalWorkSize     : return "CLBlastInvalidGlobalWorkSize";
        case CLBlastNotImplemented            : return "CLBlastNotImplemented";
        case CLBlastInvalidMatrixA            : return "CLBlastInvalidMatrixA";
        case CLBlastInvalidMatrixB            : return "CLBlastInvalidMatrixB";
        case CLBlastInvalidMatrixC            : return "CLBlastInvalidMatrixC";
        case CLBlastInvalidVectorX            : return "CLBlastInvalidVectorX";
        case CLBlastInvalidVectorY            : return "CLBlastInvalidVectorY";
        case CLBlastInvalidDimension          : return "CLBlastInvalidDimension";
        case CLBlastInvalidLeadDimA           : return "CLBlastInvalidLeadDimA";
        case CLBlastInvalidLeadDimB           : return "CLBlastInvalidLeadDimB";
        case CLBlastInvalidLeadDimC           : return "CLBlastInvalidLeadDimC";
        case CLBlastInvalidIncrementX         : return "CLBlastInvalidIncrementX";
        case CLBlastInvalidIncrementY         : return "CLBlastInvalidIncrementY";
        case CLBlastInsufficientMemoryA       : return "CLBlastInsufficientMemoryA";
        case CLBlastInsufficientMemoryB       : return "CLBlastInsufficientMemoryB";
        case CLBlastInsufficientMemoryC       : return "CLBlastInsufficientMemoryC";
        case CLBlastInsufficientMemoryX       : return "CLBlastInsufficientMemoryX";
        case CLBlastInsufficientMemoryY       : return "CLBlastInsufficientMemoryY";
        case CLBlastInsufficientMemoryTemp    : return "CLBlastInsufficientMemoryTemp";
        case CLBlastInvalidBatchCount         : return "CLBlastInvalidBatchCount";
        case CLBlastInvalidOverrideKernel     : return "CLBlastInvalidOverrideKernel";
        case CLBlastMissingOverrideParameter  : return "CLBlastMissingOverrideParameter";
        case CLBlastInvalidLocalMemUsage      : return "CLBlastInvalidLocalMemUsage";
        case CLBlastNoHalfPrecision           : return "CLBlastNoHalfPrecision";
        case CLBlastNoDoublePrecision         : return "CLBlastNoDoublePrecision";
        case CLBlastInvalidVectorScalar       : return "CLBlastInvalidVectorScalar";
        case CLBlastInsufficientMemoryScalar  : return "CLBlastInsufficientMemoryScalar";
        case CLBlastDatabaseError             : return "CLBlastDatabaseError";
        case CLBlastUnknownError              : return "CLBlastUnknownError";
        case CLBlastUnexpectedError           : return "CLBlastUnexpectedError";
        default                               : return "Unknown CLBlastStatusCode";
    }
}

# define CLBLAST_SAFE_CALL(X)                                                               \
    do {                                                                                    \
        CLBlastStatusCode r = X;                                                            \
        if (r != CLBlastSuccess)                                                            \
            LOGGER_FATAL("`%s` failed with err=%s (%d)", #X, clblast_error_to_str(r), r);   \
    } while (0)

#endif /* __LOGGER_CLBLAST_H__ */
