#!/bin/bash
# prepare test repository
set -ex

rm -rf out/fetch_test_repo/
mkdir -p out/fetch_test_repo/
cd out/fetch_test_repo/
git clone https://github.com/dancerj/gitlstreefs.git
