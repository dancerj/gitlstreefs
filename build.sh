#!/bin/bash
# Script for running a build inside Docker container.
set -e

cd /workspace
g++ configure.cc -o configure && ./configure
ninja -t clean
ninja -k 10
