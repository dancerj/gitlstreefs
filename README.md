# gitlstreefs project

This project contains some experimental file system implemented using
FUSE, that works with git and help building.

## Building

Requires libfuse, libgit2 and zlib as build time library
dependencies. The build system depends on ninja.

```shell-session
$ sudo apt-get install \
      attr \
      coreutils \
      curl \
      file \
      fuse \
      g++ \
      git \
      libattr1-dev \
      libfuse-dev \
      libgit2-dev \
      ninja-build \
      pkg-config \
      unionfs-fuse \
      zlib1g-dev

$ g++ ./configure.cc -o configure && ./configure
$ ninja
```

## gitlstree: git file system using git ls-tree, optionally via ssh

gitlstree -- mounts a filesystem based on directory and hash, to
mountpoint.  uses git ls-tree as backend.

Takes the git reposiotory from the current working directory as of
execution and mounts to the directory given as the first parameter.

```shell-session
$ ./out/gitlstree mountpoint
$ fusermount -u mountpoint
```

To mount a local repo from a different path

```shell-session
$ ./out/gitlstree --path=dir/to/some.git mountpoint
$ fusermount -u mountpoint
```

To mount a remote repo via ssh connection, using `ssh SERVER 'cd PATH
&& git ls-tree'`:

```shell-session
$ ./out/gitlstree --ssh=server --path=repos/some.git mountpoint/
$ fusermount -u mountpoint
```

### Development

There is an integration test that can be manually ran.

```shell-session
$ ./gitlstree_test.sh
```

There is an integration test for remote ssh feature. Requires a
different host that works with ssh, and be specified as parameter.
The remote needs to have the git repository `git/gitlstreefs_remote`
that contains gitlstreefs git repository.

```shell-session
$ ./gitlstree_remote_test.sh server
```

## git-githubfs: git file system using github REST API

git-githubfs -- mounts a filesystem based on github repository. Uses
github rest API v3.

```shell-session
$ ./out/git-githubfs --user=dancerj --project=gitlstreefs mountpoint/
$ ls mountpoint/
$ cat mountpoint/README.md
$ fusermount -u mountpoint
```

### Development

There is an integration test.

```shell-session
$ ./git-githubfs_test.sh
```

#### Attaching GDB

`-d` is usually a good option so that it won't daemonize.

```shell-session
$ gdb out/git-githubfs
(gdb) run --user=torvalds --project=linux ../mountpoint -d 
```

## A git file system using libgit2

experimental/gitfs -- mounts a filesystem based on directory and hash,
to mountpoint.  uses libgit2

Takes the git repository from the current working directory as of
execution and mounts to the directory given as the first parameter.

    $ ./out/experimental/gitfs mountpoint
    $ fusermount -u mountpoint

Although this was the initial approach, compared to other
implementations this implementation is slower and consumes more
memory.

## Other file systems

### ninja file system

ninjafs -- a filesystem that lists ninja build targets, and builds on
demand.  Will return IO error when build fails, you can inspect
/ninja.log to see what the error message was.

```shell-session
$ rm out/hello_world  # for demonstration.
$ out/ninjafs mountpoint/
$ ls mountpoint/
$ file mountpoint/out/hello_world
$ ./mountpoint/out/hello_world
$ fusermount -u mountpoint
```

### cow file system

cowfs -- a filesystem that uses hardlinks and copy-on-write semantics.

A reimplementation of what cowdancer would have probably been, using
FUSE. Not quite feature complete but basic features started working.

```shell-session
$ ./out/cowfs out/cowfstmp/workdir -o nonempty \
	--lock_path=out/cowfstmp/lock \
	--underlying_path=out/cowfstmp/workdir \
	--repository=out/cowfstmp/repo
$ ls -l out/cowfstmp/workdir
$ echo "hello world" > out/cowfstmp/workdir
$ fusermount -z -u mountpoint
```

Some extra mount options are required along with running as root to
get a full system running. Namely allow_other, dev, suid. Say we have
a chroot inside out/sid-chroot/chroot:

```shell-session
$ sudo ./out/cowfs --lock_path=out/sid-chroot/lock \
      --underlying_path=out/sid-chroot/chroot \
      --repository=out/sid-chroot/repo \
      out/sid-chroot/chroot \
      -o nonempty,allow_other,dev,suid,default_permissions
```

## Copying

A BSD-style license.

## TODO

- implement gitiles file system.
