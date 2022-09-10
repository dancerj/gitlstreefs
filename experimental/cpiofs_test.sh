#!/bin/bash
set -ex
TESTDIR=out/cpiofstmp
cleanup() {
    fusermount3 -z -u $TESTDIR || true
}
cleanup
trap cleanup exit

mkdir -p $TESTDIR

# start file system
out/experimental/cpiofs \
    $TESTDIR \
    --underlying_file=./testdata/test.cpio


cat $TESTDIR/README.md
stat $TESTDIR/README.md

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

grep "Test data directory" $TESTDIR/README.md
