#!/usr/bin/env sh
set -e

if [ "$IPXE_EMBED" = "" ]; then
  echo "IPXE_EMBED missing"
  exit 1
fi

if [ "$IPXE_TARGET" = "" ]; then
  echo "IPXE_TARGET missing"
  exit 1
fi

cd /app/src

if [ "MAKE_CLEAN" != "" ]; then
  echo "make clean started.."
  make clean
fi

echo "build started.."
make bin/$IPXE_TARGET EMBED=../$IPXE_EMBED
echo "done"
