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

#          TYPE                 TYPE_REAL    CU_TYPE
modes={
    "s": ("float",              "float",    "float"),
    "c": ("float _Complex",     "float",    "cuComplex"),
    "d": ("double",             "double",   "double"),
    "z": ("double _Complex",    "double",   "cuDoubleComplex")
}

assert(precision in modes)
mode = modes[precision]

src="{}/{}.cc".format(src, kernel)
dst="{}/{}{}.cc".format(dst, precision, kernel)
cmd(
    "cat {} | sed 's/££/{}/g' | sed 's/£/{}/g' | sed 's/\\bCU_TYPE\\b/{}/g' | sed 's/\\bTYPE_REAL\\b/{}/g' | sed 's/\\bTYPE\\b/{}/g' > {}".format(
        src,
        precision.upper(),
        precision,
        mode[2],
        mode[1],
        mode[0],
        dst
    )
)
