#!/bin/bash
# Sends requests to github. Should probably not be a continuous test.
MOUNTPOINT=out/githubfs_mountpoint
cleanup() {
    fusermount -u $MOUNTPOINT || true
    rmdir ./$MOUNTPOINT || true
}
cleanup

trap cleanup exit
g++ ./configure.cc -o configure && ./configure
ninja
mkdir -p $MOUNTPOINT || true
./out/git-githubfs --user=dancerj --project=gitlstreefs $MOUNTPOINT/
ls -l $MOUNTPOINT
grep 'git' $MOUNTPOINT/README.md
grep 'For testing symlink operation' $MOUNTPOINT/testdata/symlink
echo '*** COMPLETE ***'
