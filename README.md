
This project contains some experimental file system implemented using
FUSE, that works with git and help building.

# Building #

Requires libboost-dev, libfuse, libgit2 and zlib as build time library
dependencies. The build system depends on nodejs and ninja.

    # apt-get install zlib-dev libfuse-dev libgit2-dev nodejs ninja-build libboost-dev libjson-sprit-dev curl

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

ninjafs -- a filesystem that lists ninja build targets, and builds on demand.

    $ rm out/hello_world  # for demonstration.
    $ out/ninjafs mountpoint/
    $ ls mountpoint/
    $ file mountpoint/out/hello_world
    $ ./mountpoint/out/hello_world
    $ fusermount -u mountpoint

# Copying #

A BSD-style license.
