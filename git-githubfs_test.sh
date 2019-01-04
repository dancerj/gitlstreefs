#!/bin/bash
# Sends requests to github. Should probably not be a continuous test.
cleanup() {
    fusermount -u mountpoint || true
}
cleanup

trap cleanup exit
./configure.js
ninja
./out/git-githubfs --user=dancerj --project=gitlstreefs mountpoint/
ls -l mountpoint
grep 'git' mountpoint/README.md
grep 'For testing symlink operation' mountpoint/testdata/symlink
echo '*** COMPLETE ***'
