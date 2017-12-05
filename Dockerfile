FROM ubuntu:17.04

RUN apt-get -y update
RUN apt-get -y upgrade

RUN apt-get install -y \
  gcc binutils binutils-dev make perl liblzma-dev mtools mkisofs syslinux genisoimage isolinux

WORKDIR /app

ENTRYPOINT ["/app/build.sh"]
