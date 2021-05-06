#!/bin/bash
# Integration testing for ninja file system.
set -e
TESTDIR=out/ptfstmp

cleanup() {
    fusermount3 -z -u $TESTDIR || true
}
cleanup
trap cleanup exit

mkdir $TESTDIR 2> /dev/null || true

out/ptfs $TESTDIR --underlying_path=testdata/

ls -l $TESTDIR/
if cat $TESTDIR/does_not_exist; then
    echo unexpected success
    exit 1
else
    echo 'expected'
fi

grep git $TESTDIR/README.md

