#!/bin/python

import os

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
