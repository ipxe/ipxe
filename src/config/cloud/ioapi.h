/* Work around missing PCI BIOS calls in the cut-down SeaBIOS found in
 * some AWS EC2 instances.
 */
#ifdef PLATFORM_pcbios
#undef PCIAPI_PCBIOS
#define PCIAPI_CLOUD
#endif
