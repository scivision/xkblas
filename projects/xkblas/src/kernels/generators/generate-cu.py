#!/bin/python

import os
import sys

if len(sys.argv) != 4:
    print("usage: {} [ROOT-DIR] [KERNEL] [PRECISION]".format(sys.argv[0]))
    sys.exit(0)

root=sys.argv[1]
kernel=sys.argv[2]
precision=sys.argv[3]

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

src="{}/src/kernels/{}.cu".format(root, kernel)
dst="{}/src/kernels/generated/{}{}.cu".format(root, precision, kernel)
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
