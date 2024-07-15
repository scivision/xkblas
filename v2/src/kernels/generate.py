#!/bin/python

import os

# sed files
files=[
    "gemm.cc",
    "kernel.h"
]

modes=[
    ("z", "double _Complex"),
    ("c", "float _Complex"),
    ("d", "double"),
    ("s", "float")
]

for f in files:
    for mode in modes:
        cmd="cat {} | sed 's/£/{}/g' | sed 's/TYPE/{}/g' > {}{}".format(f, mode[0], mode[1], mode[0], f)
        print(cmd)
        os.system(cmd)

# move header files
for mode in modes:
    cmd="mv {}kernel.h ../../include/xkblas-{}kernel.h".format(mode[0], mode[0])
    print(cmd)
    os.system(cmd)
