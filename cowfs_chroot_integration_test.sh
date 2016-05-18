#!/bin/bash
# A simple test that uses a chroot.
# prerequisite is that we have a debootstrap directory under out/sid-chroot/chroot.

# mkdir -p out/sid-chroot/chroot out/sid-chroot/repo
# mkdir -p out/sid-chroot-from
# sudo mount -o bind /var/cache/pbuilder/sid-chroot/ out/sid-chroot-from
# sudo cp -a out/sid-chroot-from/* out/sid-chroot/chroot/

sudo ./out/cowfs -d --lock_path=out/sid-chroot/lock \
     --underlying_path=out/sid-chroot/chroot \
     --repository=out/sid-chroot/repo out/sid-chroot/chroot \
     -o nonempty,allow_other,dev,suid,default_permissions
