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
    "s": ("float",              "float"),
    "c": ("float _Complex",     "cuComplex"),
    "d": ("double",             "double"),
    "z": ("double _Complex",    "cuDoubleComplex")
}

assert(precision in modes)
mode = modes[precision]

src="{}/{}.cc".format(src, kernel)
dst="{}/{}{}.cc".format(dst, precision, kernel)
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
