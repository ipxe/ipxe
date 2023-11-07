#!/bin/bash
# Build iPXE floppy and iso with efi support images bootable in both UEFI and Legacy mode

set -e

ARCH="${ARCH:-"x86_64"}"
if [ ${ARCH} = "arm64" ] ; then
  export CROSS_COMPILE=aarch64-linux-gnu-
  iso_sources=""
fi

EMBED="${EMBED:-"ipxe-autoboot.txt"}"
target="$(echo ${EMBED%.txt} | sed 's!-ipxe!!; s!ipxe-!!')"

echo "Compiling ${ARCH} bootloaders with target: ${target}...."
sleep 2

set -x
make clean
[ ${ARCH} = "arm64" ] || make EMBED="${EMBED}" bin-${ARCH}-pcbios/ipxe--ecm--ncm.lkrn -j $(nproc) | grep "FINISH"
make EMBED="${EMBED}" bin-${ARCH}-efi/ipxe--ecm--ncm.efi -j $(nproc) | grep "FINISH"
set +x

mkdir -p "../binaries/${ARCH}/"

echo "Generating ISO\\IMG images...."
set -x
./util/genfsimg -o ../binaries/${ARCH}/ipxe-${target}.iso bin-${ARCH}-efi/ipxe--ecm--ncm.efi $([ ${ARCH} = "arm64" ] || echo "bin-${ARCH}-pcbios/ipxe--ecm--ncm.lkrn")
[ ${ARCH} = "arm64" ] || ./util/genfsimg -o ../binaries/${ARCH}/ipxe-${target}.img bin-${ARCH}-pcbios/ipxe--ecm--ncm.lkrn
set +x

cp bin-${ARCH}-efi/ipxe--ecm--ncm.efi ../binaries/${ARCH}/ipxe-${target}.efi
[ ${ARCH} = "arm64" ] || cp bin-${ARCH}-pcbios/ipxe--ecm--ncm.lkrn ../binaries/${ARCH}/ipxe-${target}.lkrn

[ ${ARCH} = "arm64" ] || echo "Bios lkrn image generated and saved to ../binaries/${ARCH}/ipxe-${target}.lkrn"
echo "EFI generated and saved to ../binaries/${ARCH}/ipxe-${target}.efi"
echo "ISO generated and saved to ../binaries/${ARCH}/ipxe-${target}.iso"
[ ${ARCH} = "arm64" ] || echo "Floppy image generated and saved to ../binaries/${ARCH}/ipxe-${target}.img"

