#!/bin/bash
# prepare test repository
set -ex

if ! [ -d out/fetch_test_repo/ ]; then
    mkdir -p out/fetch_test_repo/
    cd out/fetch_test_repo/
    git clone https://github.com/dancerj/gitlstreefs.git
fi
