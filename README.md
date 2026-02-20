iPXE network bootloader
=======================

[![Build](https://img.shields.io/github/actions/workflow/status/ipxe/ipxe/build.yml)](https://github.com/ipxe/ipxe/actions/workflows/build.yml?query=branch%3Amaster)
[![Coverity](https://img.shields.io/coverity/scan/12130)](https://scan.coverity.com/projects/ipxe-ipxe)
[![Release](https://img.shields.io/github/v/release/ipxe/ipxe)](https://github.com/ipxe/ipxe/releases/latest)

iPXE is the leading open source network boot firmware. It provides a
full PXE implementation enhanced with additional features such as:

 - boot from a web server via HTTP or [HTTPS][crypto],

 - boot from an iSCSI, FCoE, or AoE [SAN][sanboot],

 - control the boot process with a [script][scripting],

 - create interactive [forms][forms] and [menus][menus].

You can use iPXE to replace the existing PXE ROM on your network card,
or you can chainload into iPXE to obtain the features of iPXE without
the hassle of reflashing.

iPXE is free, open-source software licensed under the GNU GPL (with
some portions under GPL-compatible licences).

You can download the [rolling release binaries][rolling] (built from
the latest commit), or use the most recent [stable release][release].

For full documentation, visit the [iPXE website][ipxe].


[crypto]: https://ipxe.org/crypto
[forms]: https://ipxe.org/cmd/present
[ipxe]: https://ipxe.org
[menus]: https://ipxe.org/cmd/choose
[release]: https://github.com/ipxe/ipxe/releases/latest
[rolling]: https://boot.ipxe.org
[sanboot]: https://ipxe.org/cmd/sanboot
[scripting]: https://ipxe.org/scripting
