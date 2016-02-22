#!/bin/bash
set -ex
TESTDIR=out/cowfstmp
cleanup() {
    fusermount -z -u $TESTDIR/workdir || true
}
cleanup
trap cleanup exit

mkdir -p $TESTDIR/{workdir,repo}

# preparation before test.
cp README.md $TESTDIR/workdir/
rm -f $TESTDIR/workdir/new_file
echo old > $TESTDIR/workdir/existing_file

# start file system
out/cowfs $TESTDIR/workdir -o nonempty \
	  --lock_path=out/cowfstmp/lock \
	  --underlying_path=out/cowfstmp/workdir \
	  --repository=out/cowfstmp/repo
# out/cowfs $TESTDIR/workdir -o nonempty -d &
# sleep 1

diff README.md $TESTDIR/workdir/README.md
touch $TESTDIR/workdir/new_file
grep old $TESTDIR/workdir/existing_file
touch $TESTDIR/workdir/existing_file
echo -n new > $TESTDIR/workdir/existing_file
grep new $TESTDIR/workdir/existing_file
echo -n newer >> $TESTDIR/workdir/existing_file
cat $TESTDIR/workdir/existing_file
grep newnewer $TESTDIR/workdir/existing_file

mv $TESTDIR/workdir/new_file{,2}
rm $TESTDIR/workdir/new_file2

mkdir $TESTDIR/workdir/new_dir
rmdir $TESTDIR/workdir/new_dir

ln -sf existing_file $TESTDIR/workdir/newsymlink
ls -l $TESTDIR/workdir/newsymlink
grep newnewer $TESTDIR/workdir/newsymlink

df -h $TESTDIR/workdir/
