#!/bin/bash
set -ex
cleanup() {
    fusermount -u mountpoint2 || true
    fusermount -u mountpoint || true
    rm -rf tmp
}
cleanup
mkdir mountpoint2 tmp || true
./configure.js
ninja
./out/gitlstree mountpoint/
unionfs-fuse tmp=RW:mountpoint mountpoint2

# Start of building the first copy.
cd mountpoint2
mkdir out
./configure.js
ninja -k10 || true
ninja out/hello_world
./out/hello_world
cd ..

cleanup
