#!/bin/bash
set -ex
cleanup() {
    fusermount -u -z mountpoint2 || true
    fusermount -u -z mountpoint || true
    rm -rf tmp
    rmdir mountpoint || true
    rmdir mountpoint2 || true
}

if [ -z "$1" ]; then
    echo "Please set parameter to be the server name of the ssh remote that has git/gitlstreefs."
    exit 1
else
    echo "Trying connecting to remote server"
    ssh "$1" uname -a
fi

cleanup
mkdir mountpoint mountpoint2 tmp || true
./configure.js
ninja
trap cleanup exit
rm -rf .cache
./out/gitlstree "--ssh=$1" --path=git/gitlstreefs_remote mountpoint/

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
    ./configure.js
    time ninja -k10 -j10 out/hello_world
    time ninja out/hello_world out/gitlstree out/git-githubfs
    ./out/hello_world | grep 'Hello World'
)
echo '*** COMPLETE ***'
