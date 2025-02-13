#!/bin/python

import os
import sys

if len(sys.argv) != 3:
    print("usage: {} [ROOT-DIR] [PRECISION]".format(sys.argv[0]))
    sys.exit(0)

root=sys.argv[1]
precision=sys.argv[2]

def cmd(s):
    print(s)
    os.system(s)

modes={
    "s": ("float",              "float"),
    "c": ("float _Complex",     "cuComplex"),
    "d": ("double",             "double"),
    "z": ("double _Complex",    "cuDoubleComplex")
}

assert(precision in modes)
mode = modes[precision]

src="{}/src/kernels/kernels.h".format(root)
dst="{}/include/xkblas/{}kernels.h".format(root, precision)
cmd(
    "cat {} | sed 's/££/{}/g' | sed 's/£/{}/g' | sed 's/\\bCU_TYPE\\b/{}/g' | sed 's/\\bTYPE\\b/{}/g' > {}".format(
        src,
        precision.upper(),
        precision,
        mode[1],
        mode[0],
        dst
    )
)
