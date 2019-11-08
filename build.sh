#!/bin/bash
# Script for running a build inside Docker container.
set -e

cd /workspace
./configure.js
ninja -t clean
ninja -k 10
