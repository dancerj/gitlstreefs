FROM debian

RUN apt-get update
RUN apt-get install -yq \
      attr \
      curl \
      fuse \
      libattr1-dev \
      libboost-dev \
      libboost-filesystem-dev \
      libfuse-dev \
      libgit2-dev \
      ninja-build \
      nodejs \
      unionfs-fuse \
      zlib1g-dev
RUN apt-get install -yq g++
RUN apt-get install -yq coreutils
RUN apt-get install -yq git

COPY . src/
RUN cd src/ && git clean -f -x
RUN cd src/ && ./configure.js --nofuse
RUN cd src/ && ninja -k 10
