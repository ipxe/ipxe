#!/bin/bash
# Build iPXE floppy and iso with efi support images bootable in both UEFI and Legacy mode

set -e

make clean
make "$@"
make "$@" bin-x86_64-efi/ipxe.efi

./util/genfsimg -o ../ipxe-with-efi.iso bin-x86_64-efi/ipxe.efi bin/ipxe.lkrn
./util/genfsimg -o ../ipxe-floppy.img bin/ipxe.lkrn

echo "ISO generated and saved to ../ipxe-with-efi.iso"
echo "Floppy image generated and saved to ../ipxe-floppy.img"
