# Yandex netboot custom ipxe

The clone of official iPXE repo with patches for netboot, setup/figaro and eine services.

See [wiki](https://wiki.yandex-team.ru/dca/services/netboot/tech/ipxe/) for more information.

### Build
Use shell script `make-iso-floppy.sh` to build or update ipxe binaries and ISO images
```shell
 cd src && ./make-iso-floppy.sh
```
Run without arguments will generate `x86_64` binaries and ISO images 
using `autoboot.txt` as default boot script.

To build binaries for other archs use `ARCH` environment variable:
```shell
 ARCH=arm64 cd src && ./make-iso-floppy.sh
```

To build binaries with custom boot script use `EMBED` environment variable:
```shell
 EMBED=ipxe-shell.txt cd src && ./make-iso-floppy.sh
```

You can use both `ARCH` and `EMBED` at the same time:
```shell
 ARCH=arm64 EMBED=ipxe-shell.txt cd src && ./make-iso-floppy.sh
```