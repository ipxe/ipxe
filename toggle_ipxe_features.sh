#!/bin/bash

echo Enable VLAN command: https://ipxe.org/buildcfg/vlan_cmd
sed -i -E 's://#define([[:space:]]+)VLAN_CMD:#define\1VLAN_CMD:' src/config/general.h
grep VLAN_CMD src/config/general.h

echo Enable NTP command: https://ipxe.org/buildcfg/ntp_cmd
sed -i -E 's://#define([[:space:]]+)NTP_CMD:#define\1NTP_CMD:' src/config/general.h
grep NTP_CMD src/config/general.h

echo Enable TIME command: https://ipxe.org/buildcfg/time_cmd
sed -i -E 's://#define([[:space:]]+)TIME_CMD:#define\1TIME_CMD:' src/config/general.h
grep TIME_CMD src/config/general.h

echo Enable PCI_CMD command: https://ipxe.org/buildcfg/pci_cmd
sed -i -E 's://#define([[:space:]]+)PCI_CMD:#define\1PCI_CMD:' src/config/general.h
grep PCI_CMD src/config/general.h

echo Enable REBOOT_CMD command: https://ipxe.org/buildcfg/REBOOT_CMD
sed -i -E 's://#define([[:space:]]+)REBOOT_CMD:#define\1REBOOT_CMD:' src/config/general.h
grep REBOOT_CMD src/config/general.h

echo Enable NEIGHBOUR command: https://ipxe.org/buildcfg/neighbour_cmd
sed -i -E 's://#define([[:space:]]+)NEIGHBOUR_CMD:#define\1NEIGHBOUR_CMD:' src/config/general.h
grep NEIGHBOUR_CMD src/config/general.h

echo Enable CONSOLE command: https://ipxe.org/buildcfg/console_cmd
sed -i -E 's://#define([[:space:]]+)CONSOLE_CMD:#define\1CONSOLE_CMD:' src/config/general.h
grep CONSOLE_CMD src/config/general.h

echo Enable IMAGE command: https://ipxe.org/buildcfg/image_png
sed -i -E 's://#define([[:space:]]+)IMAGE_PNG:#define\1IMAGE_PNG:' src/config/general.h
grep IMAGE_PNG src/config/general.h

echo Enable IMAGE_TRUST_CMD command: https://ipxe.org/buildcfg/image_trust_cmd
sed -i -E 's://#define([[:space:]]+)IMAGE_TRUST_CMD:#define\1IMAGE_TRUST_CMD:' src/config/general.h
grep IMAGE_TRUST_CMD src/config/general.h

echo Enable NSLOOKUP_CMD command: https://ipxe.org/buildcfg/nslookup_cmd
sed -i -E 's://#define([[:space:]]+)NSLOOKUP_CMD:#define\1NSLOOKUP_CMD:' src/config/general.h
grep NSLOOKUP_CMD src/config/general.h

echo Enable PING_CMD command: https://ipxe.org/buildcfg/ping_cmd
sed -i -E 's://#define([[:space:]]+)PING_CMD:#define\1PING_CMD:' src/config/general.h
grep PING_CMD src/config/general.h

echo Enable CONSOLE_FRAMEBUFFER command: https://ipxe.org/buildcfg/console_framebuffer
sed -i -E 's://#define([[:space:]]+)CONSOLE_FRAMEBUFFER:#define\1CONSOLE_FRAMEBUFFER:' src/config/console.h
grep CONSOLE_FRAMEBUFFER src/config/console.h
