#!/bin/bash
set -ex
TESTDIR=out/cowfsloadtmp
cleanup() {
    fusermount3 -z -u $TESTDIR/workdir || true
}
cleanup
trap cleanup exit

rm -rf $TESTDIR/{shadow,workdir,repo}
mkdir -p $TESTDIR/{shadow,workdir,repo}

# start file system
out/cowfs $TESTDIR/workdir \
	  --lock_path=$TESTDIR/lock \
	  --underlying_path=$TESTDIR/shadow \
	  --repository=$TESTDIR/repo
sleep 1

out/experimental/parallel_writer $TESTDIR/workdir 100 100
