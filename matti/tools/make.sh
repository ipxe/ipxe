#!/usr/bin/env bash
set -euo pipefail
set -x

cp /matti/configs/general.h /src/config/general.h
make -j$(nproc) bin/ipxe.$1 EMBED=/matti/embeds/$2.ipxe

cp bin/ipxe.usb /matti/output

echo "
  OK"
