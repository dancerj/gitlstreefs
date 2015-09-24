
This project contains some experimental file system implemented using
FUSE, that works with git and help building.

# git file system using libgit2 #

gitfs -- mounts a filesystem based on directory and hash, to mountpoint.
uses libgit2

# git file system using git ls-tree #

gitlstree -- mounts a filesystem based on directory and hash, to
mountpoint.  uses git ls-tree as backend.

# ninja file system #

ninjafs -- a filesystem that lists ninja build targets, and builds on demand.

  # ninjafs mountpoint/

