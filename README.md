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
      fuse3 \
      g++ \
      git \
      libattr1-dev \
      libfuse3-dev \
      libgit2-dev \
      ninja-build \
      pkg-config \
      unionfs-fuse \
      zlib1g-dev

$ g++ ./configure.cc -o configure && ./configure
$ ninja
```

### Container images

`Dockerfile` provides configuration for container images. A sample is
provided as `podman_build.sh` as for how to use it.

### Continuous integration

Google Cloud Build service is used for running continuous
integration. Configuration file is in `cloudbuild.yaml` and
`Dockerfile`.

## gitlstree: git file system using git ls-tree, optionally via ssh

gitlstree -- mounts a filesystem based on directory and hash, to
mountpoint.  uses git ls-tree as backend.

Takes the git reposiotory from the current working directory as of
execution and mounts to the directory given as the first parameter.

```shell-session
$ ./out/gitlstree mountpoint
$ fusermount3 -u mountpoint
```

To mount a local repo from a different path

```shell-session
$ ./out/gitlstree --path=dir/to/some.git mountpoint
$ fusermount3 -u mountpoint
```

To mount a remote repo via ssh connection, using `ssh SERVER 'cd PATH
&& git ls-tree'`:

```shell-session
$ ./out/gitlstree --ssh=server --path=repos/some.git mountpoint/
$ fusermount3 -u mountpoint
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
$ fusermount3 -u mountpoint
```

### Development

There is an integration test.

```shell-session
$ ./git-githubfs_test.sh
```

#### Attaching GDB

`-d` is usually a good option so that it won't daemonize.

```shell-session
$ gdb -ex=r --args out/git-githubfs \
  --user=torvalds --project=linux ../mountpoint -d
```

## A git file system using libgit2

experimental/gitfs -- mounts a filesystem based on directory and hash,
to mountpoint.  uses libgit2

Takes the git repository from the current working directory as of
execution and mounts to the directory given as the first parameter.

```shell-session
$ ./out/experimental/gitfs mountpoint
$ fusermount3 -u mountpoint
```

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
$ fusermount3 -u mountpoint
```

### cow file system

cowfs -- a filesystem that uses hardlinks and copy-on-write semantics.

A reimplementation of what cowdancer would have probably been, using
FUSE. Not quite feature complete but basic features started working.

```shell-session
$ ./out/cowfs out/cowfstmp/workdir \
	--lock_path=out/cowfstmp/lock \
	--underlying_path=out/cowfstmp/workdir \
	--repository=out/cowfstmp/repo
$ ls -l out/cowfstmp/workdir
$ echo "hello world" > out/cowfstmp/workdir
$ fusermount3 -z -u mountpoint
```

Some extra mount options are required along with running as root to
get a full system running. Namely allow_other, dev, suid. Say we have
a chroot inside out/sid-chroot/chroot:

```shell-session
$ sudo ./out/cowfs --lock_path=out/sid-chroot/lock \
      --underlying_path=out/sid-chroot/chroot \
      --repository=out/sid-chroot/repo \
      out/sid-chroot/chroot \
      -o allow_other,dev,suid,default_permissions
```

## A cpio file system.

cpiofs -- a filesystem that mounts cpio files.

Allows mounting things like initramfs.

```shell-session
$ zcat /boot/initramfs... > /tmp/cpio-file
$ ./out/experimental/cpiofs mountpoint/ --underlying_file=/tmp/cpio-file
$ ls -l mountpoint
```

## Copying

A BSD-style license.

## TODO

- implement gitiles file system.
