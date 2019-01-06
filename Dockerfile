FROM debian

RUN apt-get clean && apt-get update
RUN apt-get install -yq \
      attr \
      coreutils \
      curl \
      fuse \
      g++ \
      git \
      libattr1-dev \
      libboost-dev \
      libboost-filesystem-dev \
      libfuse-dev \
      libgit2-dev \
      ninja-build \
      nodejs \
      unionfs-fuse \
      zlib1g-dev
COPY . src/
RUN cd src/ && \
    git clean -f -x && \
    ./configure.js --nofuse
RUN cd src/ && ninja -k 10
