#!/bin/bash
set -ex
TESTDIR=out/cowfsloadtmp
cleanup() {
    fusermount -z -u $TESTDIR/workdir || true
}
cleanup
trap cleanup exit

rm -rf $TESTDIR/{shadow,workdir,repo}
mkdir -p $TESTDIR/{shadow,workdir,repo}

# start file system
out/cowfs $TESTDIR/workdir -o nonempty \
	  --lock_path=$TESTDIR/lock \
	  --underlying_path=$TESTDIR/shadow \
	  --repository=$TESTDIR/repo \
	  -d &
sleep 1

cd $TESTDIR/workdir
../../experimental/parallel_writer
