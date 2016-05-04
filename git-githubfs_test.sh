#!/bin/bash
# Sends requests to github. Should probably not be a continuous test.
./out/git-githubfs --user=dancerj --project=gitlstreefs mountpoint/
ls -l mountpoint
grep git mountpoint/README.md
fusermount -u mountpoint
