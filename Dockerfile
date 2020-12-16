FROM ubuntu:20.04

RUN apt-get update && apt-get upgrade -y

RUN apt-get update && apt-get install -y \
  gcc binutils binutils-dev make perl liblzma-dev mtools mkisofs syslinux genisoimage isolinux

WORKDIR /src
