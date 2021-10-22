#!/bin/sh
# script to invoke podman and run the build locally.
set -e

invoke() {
    echo "========================================"
    echo "$@"
    echo "========================================"
    podman run -it --rm \
	   --device=/dev/fuse:/dev/fuse:rw \
	   -v $(pwd):$(pwd):rw \
	   -w $(pwd) \
	   --privileged=true \
	   --entrypoint=/bin/sh gitlstreefsbuilder \
	   -c "$@"
}

podman build . -t gitlstreefsbuilder

invoke "g++ configure.cc -o configure  && ./configure && ninja -t clean && ninja -k 10"
invoke ./gitlstree_test.sh
invoke ./cowfs_test.sh
invoke ./ninjafs_test.sh
invoke ./git-githubfs_test.sh
