#!/bin/bash
# Script for running a build inside Docker container.
set -e

cd /workspace
git fetch --unshallow || true
./configure.js
ninja -t clean
ninja -k 10
