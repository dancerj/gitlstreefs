FROM debian

RUN apt-get update && \
    apt-get install -yq \
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
    nodejs \
    unionfs-fuse \
    zlib1g-dev && \
    apt-get clean
COPY . src/
RUN cd src/ && \
    ./configure.js && \
    ninja -t clean && \
    ninja -k 10
