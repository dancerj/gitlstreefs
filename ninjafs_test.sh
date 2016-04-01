#!/bin/bash
# Integration testing for ninja file system.
set -e
TESTDIR=out/ninjafstemp

cleanup() {
    fusermount -z -u $TESTDIR || true
}
cleanup
trap cleanup exit

mkdir $TESTDIR || true

(
    cd ./testdata/failninja/ &&
	../../out/ninjafs ../../$TESTDIR -o attr_timeout=0
)
ls -l $TESTDIR/
if cat $TESTDIR/does_not_exist; then
    echo unexpected success
    exit 1
else
    echo 'expected'
fi

# Without attr_timeout=0, ninja.log size change is not detected for 1
# second.
file $TESTDIR/ninja.log
ls -l $TESTDIR/ninja.log
cat $TESTDIR/ninja.log
grep 'bunch of error message' $TESTDIR/ninja.log

