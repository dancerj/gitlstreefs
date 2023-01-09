#!/bin/bash
set -ex
TESTDIR=out/ext2tmp
cleanup() {
    fusermount3 -z -u $TESTDIR || true
}
cleanup
trap cleanup exit

mkdir -p $TESTDIR

# create data file
TESTDATA=out/ext2.testdata
rm -f "${TESTDATA}"
truncate --size 10M "${TESTDATA}"
/usr/sbin/mke2fs -d ./testdata/ "${TESTDATA}"

# start file system
out/experimental/ext2 \
    $TESTDIR \
    --underlying_file="${TESTDATA}"

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

echo '*** COMPLETE ***'
