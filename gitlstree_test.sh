#!/bin/bash
set -ex
cleanup() {
    sudo umount mountpoint2/out || true
    fusermount -u mountpoint2 || true
    fusermount -u mountpoint || true
    rm -rf tmp
}
cleanup
mkdir mountpoint2 tmp || true
./configure.js
ninja
./out/gitlstree mountpoint/

# Minimal testing.
ls -l mountpoint
grep 'git' mountpoint/README.md
grep 'For testing symlink operation' mountpoint/testdata/symlink

# Create a read-write portion.
unionfs-fuse tmp=RW:mountpoint mountpoint2

# Start of building the first copy.
cd mountpoint2
mkdir out
./configure.js
ninja -k10 -j10 out/hello_world
ninja out/hello_world out/gitlstree out/git-githubfs
./out/hello_world | grep 'Hello World'
cd ..

cleanup
