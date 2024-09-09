#!/bin/python

import os

kernels=[
    "gemm"
]

# sed files
files=[
    "kernel.h"
]
files += ["{}.cc".format(kernel) for kernel in kernels]

modes=[
    ("s",   "float",            "float"),
    ("c",   "float _Complex",   "cuComplex"),
    ("d",   "double",           "double"),
    ("z",   "double _Complex",  "cuDoubleComplex")
]

def cmd(s):
    print(s)
    os.system(s)

for f in files:
    for mode in modes:
        cmd("cat {} | sed 's/££/{}/g' | sed 's/£/{}/g' | sed 's/\\bCU_TYPE\\b/{}/g' | sed 's/\\bTYPE\\b/{}/g' > {}{}".format(
                f,
                mode[0].upper(),
                mode[0],
                mode[2],
                mode[1],
                mode[0],
                f
            )
        )

# move header files
for mode in modes:
    cmd("mv {}kernel.h ../../include/xkblas-{}kernel.h".format(mode[0], mode[0]))

# task format register
f = "kernel-task-format-register"
cmd("echo -n '' > {}.cc".format(f))
cmd("echo -n '' > {}.h".format(f))
for kernel in kernels:
    for mode in modes:
        cmd("echo 'register_{}{}_format();' >> {}.cc".format(mode[0], kernel, f))
        cmd("echo 'void register_{}{}_format(void);' >> {}.h".format(mode[0], kernel, f))
