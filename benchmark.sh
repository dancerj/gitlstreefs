#!/bin/bash
# Quick benchmarking.
#  sysctl kernel.perf_event_paranoid may need to be tweaked.
perf stat -r 10 ./out/base64decode_benchmark testdata/base64encoded.txt 100000
perf stat -r 10 ./out/jsonparser_util ./testdata/commits.json 1000
