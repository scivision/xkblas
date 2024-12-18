#!/bin/python

import os
import sys

if len(sys.argv) != 3:
    print("usage: {} [SRC-DIR] [INC-DIR]".format(sys.argv[0]))
    sys.exit(0)

srcdir=sys.argv[1]
incdir=sys.argv[2]

kernels=[
    "copyscale",
    "gemm",
    "gemmt",
    "symm",
    "syr2k",
    "syrk",
    "trmm",
    "trsm",
]

# sed files
files=[
    "copyscale.cu",
    "kernel.h",
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
        cmd("cat {}/{} | sed 's/££/{}/g' | sed 's/£/{}/g' | sed 's/\\bCU_TYPE\\b/{}/g' | sed 's/\\bTYPE\\b/{}/g' > {}/generated/{}{}".format(
                srcdir,
                f,
                mode[0].upper(),
                mode[0],
                mode[2],
                mode[1],
                srcdir,
                mode[0],
                f
            )
        )

# move header files
for mode in modes:
    cmd("mv {}/generated/{}kernel.h {}/{}kernel.h".format(srcdir, mode[0], incdir, mode[0]))

# task format register
f = "generated/kernel-task-format-register"
cmd("echo -n '' > {}/{}.cc".format(srcdir, f))
cmd("echo -n '' > {}/{}.h".format(srcdir, f))
for kernel in kernels:
    for mode in modes:
        cmd("echo 'register_{}{}_format();' >> {}/{}.cc".format(mode[0], kernel, srcdir, f))
        cmd("echo 'void register_{}{}_format(void);' >> {}/{}.h".format(mode[0], kernel, srcdir,f))
