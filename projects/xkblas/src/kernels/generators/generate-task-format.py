#!/bin/python

import os
import sys

if len(sys.argv) != 4:
    print("usage: {} [ROOT-DIR] [KERNELS] [PRECISIONS]".format(sys.argv[0]))
    sys.exit(0)

root=sys.argv[1]
kernels=sys.argv[2].split(" ")
precisions=sys.argv[3].split(" ")
print(kernels)

def cmd(s):
    # print(s)
    os.system(s)

dst="{}/src/kernels/generated/kernel-task-format-register.cc".format(root)
cmd("echo -n '' > {}".format(dst))

for kernel in kernels:
    for precision in precisions:
        cmd("echo 'void register_{}{}_format(void);' >> {}".format(precision, kernel, dst))

cmd("echo 'void xkblas_task_format_register(void) {}' >> {}".format('{', dst))
for kernel in kernels:
    for precision in precisions:
        cmd("echo '    register_{}{}_format();' >> {}".format(precision, kernel, dst))
cmd("echo '{}' >> {}".format('}', dst))
