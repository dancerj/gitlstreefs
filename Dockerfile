FROM debian:bullseye

RUN apt-get update && \
    apt-get install -yq \
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
    zlib1g-dev && \
    apt-get clean

COPY build.sh /build.sh
ENTRYPOINT [ "/build.sh" ]
