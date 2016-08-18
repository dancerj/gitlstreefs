
This project contains some experimental file system implemented using
FUSE, that works with git and help building.

# Building #

Requires libboost-dev, libfuse, libgit2 and zlib as build time library
dependencies. The build system depends on nodejs and ninja.

    # apt-get install \
      attr \
      curl \
      fuse \
      libattr1-dev \
      libboost-dev \
      libboost-filesystem-dev \
      libfuse-dev \
      libgit2-dev \
      libjson-spirit-dev \
      ninja-build \
      nodejs \
      unionfs-fuse \
      zlib1g-dev

    $ ./configure.js
    $ ninja

# git file system using git ls-tree, optionally via ssh #

gitlstree -- mounts a filesystem based on directory and hash, to
mountpoint.  uses git ls-tree as backend.

Takes the git reposiotory from the current working directory as of
execution and mounts to the directory given as the first parameter.

    $ ./out/gitlstree mountpoint
    $ fusermount -u mountpoint

To mount a remote repo via ssh connection, using `ssh SERVER 'cd PATH
&& git ls-tree'`:

    $ ./out/gitlstree --ssh=server --path=repos/some.git mountpoint/
    $ fusermount -u mountpoint

# git file system using github REST API #

git-githubfs -- mounts a filesystem based on github repository. Uses
github rest API v3.

    $ ./out/git-githubfs --user=dancerj --project=gitlstreefs mountpoint/
    $ ls mountpoint/
    $ cat mountpoint/README.md
    $ fusermount -u mountpoint

# git file system using libgit2 #

experimental/gitfs -- mounts a filesystem based on directory and hash,
to mountpoint.  uses libgit2

Takes the git repository from the current working directory as of
execution and mounts to the directory given as the first parameter.

    $ ./out/experimental/gitfs mountpoint
    $ fusermount -u mountpoint

Although this was the initial approach, compared to other
implementations this implementation is slower and consumes more
memory.

# ninja file system #

ninjafs -- a filesystem that lists ninja build targets, and builds on
demand.  Will return IO error when build fails, you can inspect
/ninja.log to see what the error message was.

    $ rm out/hello_world  # for demonstration.
    $ out/ninjafs mountpoint/
    $ ls mountpoint/
    $ file mountpoint/out/hello_world
    $ ./mountpoint/out/hello_world
    $ fusermount -u mountpoint

# cow file system #

cowfs -- a filesystem that uses hardlinks and copy-on-write semantics.

A reimplementation of what cowdancer would have probably been, using
FUSE. Not quite feature complete but basic features started working.

    $ ./out/cowfs out/cowfstmp/workdir -o nonempty \
    	  --lock_path=out/cowfstmp/lock \
	  --underlying_path=out/cowfstmp/workdir \
	  --repository=out/cowfstmp/repo
    $ ls -l out/cowfstmp/workdir
    $ echo "hello world" > out/cowfstmp/workdir
    $ fusermount -z -u mountpoint

Some extra mount options are required along with running as root to
get a full system running. Namely allow_other, dev, suid. Say we have
a chroot inside out/sid-chroot/chroot:

    $ sudo ./out/cowfs --lock_path=out/sid-chroot/lock \
      --underlying_path=out/sid-chroot/chroot \
      --repository=out/sid-chroot/repo \
      out/sid-chroot/chroot \
      -o nonempty,allow_other,dev,suid,default_permissions

# Copying #

A BSD-style license.
