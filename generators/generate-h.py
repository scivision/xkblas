#!/bin/python

import os
import sys

if len(sys.argv) != 4:
    print("usage: {} [SRC] [DST] [PRECISION]".format(sys.argv[0]))
    sys.exit(0)

src=sys.argv[1]
dst=sys.argv[2]
precision=sys.argv[3]

def cmd(s):
    print(s)
    os.system(s)

modes={
    "s": ("float",           "float",    "float"),
    "c": ("float _Complex",  "float",    "cuComplex"),
    "d": ("double",          "double",   "double"),
    "z": ("double _Complex", "double",   "cuDoubleComplex")
}

assert(precision in modes)
mode = modes[precision]

cmd(
    "cat {} | sed 's/£TYPE_REAL/{}/g' | sed 's/£TYPE/{}/g' | sed 's/££/{}/g' | sed 's/£/{}/g' | sed 's/\\bCU_TYPE\\b/{}/g' > {}".format(
        src,
        mode[1],
        mode[0],
        precision.upper(),
        precision,
        mode[2],
        dst
    )
)
