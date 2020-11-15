#!/bin/bash
set -ex
cleanup() {
    fusermount -u -z mountpoint2 || true
    fusermount -u -z mountpoint || true
    rm -rf tmp
    rmdir mountpoint || true
    rmdir mountpoint2 || true
}
cleanup
mkdir mountpoint mountpoint2 tmp || true
g++ ./configure.cc -o configure && ./configure
ninja
trap cleanup exit
rm -rf .cache
./out/gitlstree \
    --path=out/fetch_test_repo/gitlstreefs \
    mountpoint/ \
    -d > out/log.gitlstree_test.sh 2>&1 &
sleep 1

# Minimal testing.
ls -l mountpoint
grep 'git' mountpoint/README.md
grep 'For testing symlink operation' mountpoint/testdata/symlink

# Create a read-write portion.
unionfs-fuse tmp=RW:mountpoint mountpoint2

# Start of building the first copy.
(
    set -ex
    cd mountpoint2
    mkdir out
    g++ ./configure.cc -o configure && ./configure
    ninja -k10 -j10 out/hello_world
    ninja out/hello_world out/gitlstree out/git-githubfs
    ./out/hello_world | grep 'Hello World'
)
echo '*** COMPLETE ***'
