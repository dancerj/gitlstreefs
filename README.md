
This project contains some experimental file system implemented using
FUSE, that works with git and help building.

# Building #

Requires libboost-dev, libfuse, libgit2 and zlib as build time library
dependencies. The build system depends on nodejs and ninja.

    # apt-get install zlib-dev libfuse-dev libgit2-dev nodejs ninja-build libboost-dev libjson-sprit-dev

    $ ./configure.js
    $ ninja

# git file system using libgit2 #

gitfs -- mounts a filesystem based on directory and hash, to mountpoint.
uses libgit2

Takes the git reposiotory from the current working directory as of
execution and mounts to the directory given as the first parameter.

    $ ./out/gitfs mountpoint
    $ fusermount -u mountpoint

# git file system using git ls-tree #

gitlstree -- mounts a filesystem based on directory and hash, to
mountpoint.  uses git ls-tree as backend.

Takes the git reposiotory from the current working directory as of
execution and mounts to the directory given as the first parameter.

    $ ./out/gitlstree mountpoint
    $ fusermount -u mountpoint

# git file system using github REST API #

git-githubfs -- mounts a filesystem based on github repository. Uses
github rest API v3.

	$ ./out/git-githubfs --user=dancerj --project=gitlstreefs mountpoint/
	$ ls mountpoint/
	$ cat mountpoint/README.md
    $ fusermount -u mountpoint

# ninja file system #

ninjafs -- a filesystem that lists ninja build targets, and builds on demand.

	$ rm out/hello_world  # for demonstration.
	$ out/ninjafs mountpoint/
	$ ls mountpoint/
	$ file mountpoint/out_hello_world
	$ ./mountpoint/out_hello_world
	$ fusermount -u mountpoint


# Copying #

A BSD-style license.
