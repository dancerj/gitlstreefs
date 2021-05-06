#!/bin/bash
set -ex
TESTDIR=out/globfstmp
cleanup() {
    fusermount3 -z -u $TESTDIR || true
}
cleanup
trap cleanup exit

mkdir -p $TESTDIR

# start file system
out/experimental/globfs \
    $TESTDIR \
    --glob_pattern='c*' \
    --underlying_path=./ 

ls -l $TESTDIR
if cat $TESTDIR/COPYING; then
    exit 1  # shouldn't be possible to read this file.
else
    echo "Failure is success."
fi

if stat $TESTDIR/COPYING; then
    exit 1  # shouldn't be possible to read this file.
else
    echo "Failure is success."
fi

grep cowfs $TESTDIR/cowfs.cc
