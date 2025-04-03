#!/bin/python

import os
import sys

print(sys.argv)
print(len(sys.argv))

if len(sys.argv) != 9:
    print("usage: {} [DST-DIR] [DST-FILE] [USE_CUDA] [USE_ZE] [USE_SYCL] [USE_CL] [USE_STATS] [USE_CAIRO]".format(sys.argv[0]))
    sys.exit(0)

dstdir=sys.argv[1]
dstfile=sys.argv[2]

def cmd(s):
    print(s)
    return os.system(s)

def cmake_option_convert(opt):
    opt = opt.lower()
    if opt == "off" or opt == "false" or opt == "0":
        return 0
    return 1

use_cuda=cmake_option_convert(sys.argv[3])
use_ze=cmake_option_convert(sys.argv[4])
use_sycl=cmake_option_convert(sys.argv[5])
use_opencl=cmake_option_convert(sys.argv[6])
use_stats=cmake_option_convert(sys.argv[7])
use_cairo=cmake_option_convert(sys.argv[8])
cmd('sed -e "s/__XKRT_SUPPORT_CUDA_VALUE__/{}/g" -e "s/__XKRT_SUPPORT_ZE_VALUE__/{}/g" -e "s/__XKRT_SUPPORT_SYCL_VALUE__/{}/g" -e "s/__XKRT_SUPPORT_CL_VALUE__/{}/g" -e "s/__XKRT_SUPPORT_STATS_VALUE__/{}/g" -e "s/__XKRT_SUPPORT_CAIRO_VALUE__/{}/g" {}/{}.template > {}/{}-tmp.h'.format(use_cuda, use_ze, use_sycl, use_opencl, use_stats, use_cairo, dstdir, dstfile, dstdir, dstfile))


if cmd("test -f {}/{}.h".format(dstdir, dstfile)) or cmd("diff {}/{}.h {}/{}-tmp.h".format(dstdir, dstfile, dstdir, dstfile)):
    cmd("cp {}/{}-tmp.h {}/{}.h".format(dstdir, dstfile, dstdir, dstfile))

cmd("rm {}/{}-tmp.h".format(dstdir, dstfile))
