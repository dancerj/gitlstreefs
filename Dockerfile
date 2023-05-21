FROM debian:bullseye-slim

RUN apt-get update && \
    apt-get install -yq \
    attr \
    coreutils \
    curl \
    e2fsprogs \
    file \
    fuse3 \
    g++ \
    git \
    libattr1-dev \
    libfuse3-dev \
    libgcrypt-dev \
    libgit2-dev \
    ninja-build \
    pkg-config \
    unionfs-fuse \
    zlib1g-dev && \
    apt-get clean

COPY build.sh /build.sh
ENTRYPOINT [ "/build.sh" ]
