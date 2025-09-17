#!/bin/python

import os
import sys

if len(sys.argv) != 5:
    print("usage: {} [SRC-DIR] [DST-DIR] [KERNEL] [PRECISION]".format(sys.argv[0]))
    sys.exit(0)

src=sys.argv[1]
dst=sys.argv[2]
kernel=sys.argv[3]
precision=sys.argv[4]

def cmd(s):
    print(s)
    os.system(s)

modes={
    "s": ("float",              "float",            "float",  "UNUSED",    "UNUSED"),
    "c": ("float _Complex",     "hipFloatComplex",  "float",  "hipCrealf", "hipCimagf"),
    "d": ("double",             "double",           "double", "UNUSED",    "UNUSED"),
    "z": ("double _Complex",    "hipDoubleComplex", "double", "hipCreal",  "hipCimag")
}

assert(precision in modes)
mode = modes[precision]

src="{}/{}.hip.cpp".format(src, kernel)
dst="{}/{}{}.hip.cpp".format(dst, precision, kernel)
cmd(
    "cat {} | sed 's/££/{}/g' | sed 's/£/{}/g' | sed 's/HIP_TYPE/{}/g' | sed 's/HIP_REAL/{}/g' | sed 's/HIP_IMAG/{}/g' | sed 's/\\bREAL_TYPE\\b/{}/g' | sed 's/\\bTYPE\\b/{}/g' > {}".format(
        src,
        precision.upper(),
        precision,
        mode[1],
        mode[3],
        mode[4],
        mode[2],
        mode[0],
        dst
    )
)
