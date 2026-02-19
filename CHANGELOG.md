Changelog
---------

## [Unreleased]

- Add support for UEFI Secure Boot via a dedicated iPXE shim.

- Add support for LoongArch64 and RISC-V CPU architectures.

- Add initial support for bare-metal operation on RISC-V.

- Automatically download and boot `autoexec.ipxe` script if it exists.

- Construct initrd-style CPIO archive for UEFI kernels.

- Support loading a UEFI executable via a shim.

- Support EAPoL and port authentication.

- Support Link Layer Discovery Protocol (LLDP).

- Support DHE and ECDHE key exchange mechanisms, GCM cipher mode of
  operation, X25519, P-256, and P-384 elliptic curves, and ECDSA
  certificates.

- Remove support for TLS v1.0.

- Support gzip decompression and compressed arm64 kernels.

- Support decryption of CMS-encrypted files.

- Use ACPI-provided system MAC address for USB NICs.

- Support ECAM as a mechanism for accessing PCI configuration space.

- Support keyboard maps.

- Support dynamically created interactive forms.

- Extend ConnectX driver to support ConnectX-3 devices.

- Support error recovery on Broadcom NetXtreme-E devices.

- Add Intel 100 Gigabit Ethernet device driver.

- Add Marvell AQtion device driver.

- Add Cadence Gigabit Ethernet MAC (GEM) device driver.

- Add DesignWare MAC device driver.

- Add DesignWare USB3 host controller device driver.

- Add RDC R6040 device driver.

- Add Google Virtual Ethernet (GVE) device driver.

- Add EFI Managed Network Protocol device driver.

- Add libslirp-based virtual NIC device driver.

- Allow for reproducible builds.

- Publish official images for AWS and Google Cloud.

- Switch from Travis CI to GitHub Actions.

## [v1.21.1] 2020-12-31

- Create DMA API and support UEFI systems with the IOMMU enabled (for
  drivers that have been updated to the new API).

- Add iPhone USB tethering device driver.

- Add USB mass storage device driver.

- Add Broadcom NetXtreme-E device driver.

- Enable stack cookies (for UEFI).

## [v1.20.1] 2020-01-02

- See commit log for significant changes in this and any earlier
  releases.

## [v1.0.0] 2010-02-02

- Prehistoric version.


[unreleased]: https://github.com/ipxe/ipxe/commits
[v1.21.1]: https://github.com/ipxe/ipxe/releases/tag/v1.21.1
[v1.20.1]: https://github.com/ipxe/ipxe/releases/tag/v1.20.1
[v1.0.0]: https://github.com/ipxe/ipxe/releases/tag/v1.0.0
