FROM ubuntu:17.04

RUN apt-get -y update
RUN apt-get -y upgrade

RUN apt-get install -y \
  gcc binutils make perl liblzma-dev mtools mkisofs syslinux

WORKDIR /app

ENTRYPOINT ["/app/build.sh"]
