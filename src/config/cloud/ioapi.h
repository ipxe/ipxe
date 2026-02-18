/* Work around missing PCI BIOS calls in the cut-down SeaBIOS found in
 * some AWS EC2 instances.
 */
#ifdef PLATFORM_pcbios
#undef PCIAPI_PCBIOS
#define PCIAPI_CLOUD
#define PCIAPI_RUNTIME_ECAM
#define PCIAPI_RUNTIME_PCBIOS
#define PCIAPI_RUNTIME_DIRECT
#endif

/* Work around missing PCI host bridge drivers in the cut-down UEFI found
 * in some AWS EC2 instances.
 */
#ifdef PLATFORM_efi
#undef PCIAPI_EFI
#define PCIAPI_CLOUD
#define PCIAPI_RUNTIME_EFI
#define PCIAPI_RUNTIME_ECAM
#endif
