#!/bin/bash
# Sends requests to github. Should probably not be a continuous test.
./out/git-githubfs --user=dancerj --project=gitlstreefs mountpoint/
ls -l mountpoint
grep 'git' mountpoint/README.md
grep 'For testing symlink operation' mountpoint/testdata/symlink
fusermount -u mountpoint
