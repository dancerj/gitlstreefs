#!/bin/bash
set -ex
TESTDIR=out/unkofstmp
cleanup() {
    fusermount3 -z -u $TESTDIR || true
}
cleanup
trap cleanup exit

rm -rf $TESTDIR
mkdir -p $TESTDIR

# Create files under the directory
cp README.md $TESTDIR/

# start file system
out/experimental/unkofs \
    $TESTDIR

ls -l $TESTDIR
if cat $TESTDIR/DOES_NOT_EXIST; then
    exit 1  # shouldn't be possible to read this file.
else
    echo "Failure is success."
fi

grep git $TESTDIR/README.md
grep unkounko $TESTDIR/unko

echo '*** COMPLETE ***'
