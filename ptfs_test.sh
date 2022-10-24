#!/bin/bash
# Integration testing for ninja file system.
set -ex
TESTDIR=out/ptfstmp
TESTSRC=out/ptfstmpsrc

cleanup() {
    fusermount3 -z -u $TESTDIR || true
}
cleanup
trap cleanup exit

mkdir $TESTDIR 2> /dev/null || true

rm -r $TESTSRC || true
cp -r testdata $TESTSRC

out/ptfs $TESTDIR --underlying_path=$TESTSRC

out/ptfs_exercise

ls -l $TESTDIR/
if cat $TESTDIR/does_not_exist; then
    echo unexpected success
    exit 1
else
    echo 'expected'
fi

grep git $TESTDIR/README.md

touch $TESTDIR/one
echo hoge > $TESTDIR/two

if out/renameat2 $TESTDIR/two $TESTDIR/one RENAME_NOREPLACE; then
    echo unexpected success
    exit 1
else
    echo 'expected'
fi

out/renameat2 $TESTDIR/two $TESTDIR/one RENAME_EXCHANGE

ls -l $TESTDIR/
grep hoge $TESTDIR/one

fincore $TESTDIR/*
