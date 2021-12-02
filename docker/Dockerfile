# Sets up a build environment for iPXE
FROM ubuntu

RUN apt update && \
  apt install -y \
    build-essential \
    liblzma-dev \
    isolinux \
    mkisofs \
    && \
  rm -rf /var/lib/apt/lists/*

ENTRYPOINT make -j 4 -C src
