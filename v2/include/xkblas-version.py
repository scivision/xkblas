#!/bin/python

import os
import sys

if len(sys.argv) != 2:
    print("usage: {} [DST-DIR]".format(sys.argv[0]))
    sys.exit(0)

dstdir=sys.argv[1]
dstfile="xkblas-version"

def cmd(s):
    print(s)
    return os.system(s)

cmd('sed "s/£GIT_HASH/$(git describe --dirty --broken --tags --long)/g" {}/{}.template > {}/{}-tmp.h'.format(dstdir, dstfile, dstdir, dstfile))

if cmd("test -f {}/{}.h".format(dstdir, dstfile)) or cmd("diff {}/{}.h {}/{}-tmp.h".format(dstdir, dstfile, dstdir, dstfile)):
    cmd("cp {}/{}-tmp.h {}/{}.h".format(dstdir, dstfile, dstdir, dstfile))

cmd("rm {}/{}-tmp.h".format(dstdir, dstfile))
