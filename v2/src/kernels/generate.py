#!/bin/python

import os

# sed files
files=[
    "gemm.cc",
    "kernel.h"
]

modes=[
    ("b",   "char",               "TASK_PRECISION_B"),
    ("s",   "float",              "TASK_PRECISION_S"),
    ("c",   "float _Complex",     "TASK_PRECISION_C"),
    ("d",   "double",             "TASK_PRECISION_D"),
    ("z",   "double _Complex",    "TASK_PRECISION_Z")
]

for f in files:
    for mode in modes:
        cmd="cat {} | sed 's/£/{}/g' | sed 's/TYPE/{}/g' | sed 's/PRECISION/{}/g' > {}{}".format(f, mode[0], mode[1], mode[2], mode[0], f)
        print(cmd)
        os.system(cmd)

# move header files
for mode in modes:
    cmd="mv {}kernel.h ../../include/xkblas-{}kernel.h".format(mode[0], mode[0])
    print(cmd)
    os.system(cmd)
