#!/bin/bash
set -ex
cleanup() {
    fusermount3 -u -z mountpoint2 || true
    fusermount3 -u -z mountpoint || true
    rm -rf tmp tmp-work
    rmdir mountpoint || true
    rmdir mountpoint2 || true
}
cleanup
mkdir mountpoint mountpoint2 tmp tmp-work || true
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
fuse-overlayfs -o lowerdir=mountpoint,upperdir=tmp,workdir=tmp-work mountpoint2

# Start of building the first copy.
(
    set -ex
    cd mountpoint2
    mkdir out
    g++ ./configure.cc -o configure && ./configure
    ninja out/hello_world out/gitlstree out/git-githubfs
    ./out/hello_world | grep 'Hello World'
)
echo '*** COMPLETE ***'
