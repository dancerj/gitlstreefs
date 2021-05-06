#!/bin/bash
# Integration testing for ninja file system.
set -e
TESTDIR=out/ninjafstemp

cleanup() {
    fusermount3 -z -u $TESTDIR || true
}
cleanup
trap cleanup exit

mkdir $TESTDIR 2> /dev/null || true

(
    cd ./testdata/failninja/
    ninja -t clean
    # add `-d &` at end for more debugging
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

cat $TESTDIR/slow &
sleep 1
cat $TESTDIR/fast &
wait

