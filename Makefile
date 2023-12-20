#!/usr/bin/make -f
# -*- makefile -*-

# Uncomment this to turn on verbose mode.
#export DH_VERBOSE=1

TMPDIR=$(CURDIR)/tmp
LOGS_DIR=$(CURDIR)/logs

#MAKEARGS:=DEBUG="xfer,retry,ndp,dhcp,dhcpv6,dhcpopts,lldp,ipv6,efi_snp,efi_snp_hii,efi_usb,efi_debug,efi_wrap" LOG_LEVEL=LOG_ALL
#MAKEARGS:=DEBUG="device,lldp,pci,init,efi_init,efi_pci" LOG_LEVEL=LOG_ALL

# parameters
# $(1) = target, $(2) = embedded file, $(3) = output file, $(4) = architecture
define make_with_name
	mkdir -p $(dir $(3)) $(TMPDIR)/$(3) $(LOGS_DIR)
	cp -r src $(TMPDIR)/$(3)/
	+CROSS_COMPILE=$(4)-linux-gnu- $(MAKE) EMBED=$(shell basename $(2)) $(MAKEARGS) \
	  -C $(TMPDIR)/$(3)/src $(1) > $(LOGS_DIR)/$(subst /,_,$(1)).log 2>&1
	mv $(TMPDIR)/$(3)/src/$(1) $(3)
endef

all: build

# kpxe kernels
kernels/x86_64/ipxe-autoboot.lkrn: src/ipxe-autoboot-no-ipoib.txt
	$(call make_with_name,bin-x86_64-pcbios/ipxe--ecm--ncm.lkrn,$<,$@,x86_64)

kernels/x86_64/ipxe-shell.lkrn: src/ipxe-shell.txt
	$(call make_with_name,bin-x86_64-pcbios/ipxe--ecm--ncm.lkrn,$<,$@,x86_64)

# UEFI kernels
kernels/x86_64/snp.efi: src/ipxe-shell.txt
	$(call make_with_name,bin-x86_64-efi/snp.efi,$<,$@,x86_64)
kernels/x86_64/snponly.efi: src/ipxe-autoboot.txt
	$(call make_with_name,bin-x86_64-efi/snponly.efi,$<,$@,x86_64)
kernels/x86_64/ipxe-autoboot.efi: src/ipxe-autoboot-no-ipoib.txt
	$(call make_with_name,bin-x86_64-efi/ipxe--ecm--ncm.efi,$<,$@,x86_64)
kernels/x86_64/ipxe-shell.efi: src/ipxe-shell.txt
	$(call make_with_name,bin-x86_64-efi/ipxe--ecm--ncm.efi,$<,$@,x86_64)

# Arm64 UEFI kernels
kernels/arm64/snp.efi: src/ipxe-shell.txt
	$(call make_with_name,bin-arm64-efi/snp.efi,$<,$@,aarch64)
kernels/arm64/snponly.efi: src/ipxe-autoboot.txt
	$(call make_with_name,bin-arm64-efi/snponly.efi,$<,$@,aarch64)
kernels/arm64/ipxe-autoboot.efi: src/ipxe-autoboot-no-ipoib.txt
	$(call make_with_name,bin-arm64-efi/ipxe--ecm--ncm.efi,$<,$@,aarch64)
kernels/arm64/ipxe-shell.efi: src/ipxe-shell.txt
	$(call make_with_name,bin-arm64-efi/ipxe--ecm--ncm.efi,$<,$@,aarch64)


binaries/x86_64/ipxe-autoboot.iso: kernels/x86_64/ipxe-autoboot.lkrn kernels/x86_64/ipxe-autoboot.efi
	mkdir -p $(dir $@)
	$(CURDIR)/src/util/genfsimg -o $@ $^

binaries/x86_64/ipxe-autoboot.img: kernels/x86_64/ipxe-autoboot.lkrn
	mkdir -p $(dir $@)
	$(CURDIR)/src/util/genfsimg -o $@ $^

binaries/x86_64/ipxe-shell.iso: kernels/x86_64/ipxe-shell.lkrn kernels/x86_64/ipxe-shell.efi
	mkdir -p $(dir $@)
	$(CURDIR)/src/util/genfsimg -o $@ $^

binaries/x86_64/ipxe-shell.img: kernels/x86_64/ipxe-shell.lkrn
	mkdir -p $(dir $@)
	$(CURDIR)/src/util/genfsimg -o $@ $^

# arm64 ISO
binaries/arm64/ipxe-autoboot.iso: kernels/arm64/ipxe-autoboot.efi
	mkdir -p $(dir $@)
	$(CURDIR)/src/util/genfsimg -o $@ $^

binaries/arm64/ipxe-shell.iso: kernels/arm64/ipxe-shell.efi
	mkdir -p $(dir $@)
	$(CURDIR)/src/util/genfsimg -o $@ $^

clean:
	make -C src clean
	rm -rf binaries kernels $(TMPDIR)

ipxe-efi: kernels/x86_64/ipxe-autoboot.efi kernels/arm64/ipxe-autoboot.efi
snp: kernels/x86_64/snp.efi kernels/arm64/snp.efi
snponly: kernels/x86_64/snponly.efi kernels/arm64/snponly.efi
build-arm64: binaries/arm64/ipxe-autoboot.iso binaries/arm64/ipxe-shell.iso

build-img: binaries/x86_64/ipxe-autoboot.img binaries/x86_64/ipxe-shell.img
build-iso: binaries/x86_64/ipxe-autoboot.iso binaries/x86_64/ipxe-shell.iso build-arm64
build: snponly build-img build-iso

.phony: clean build build-arm64 build-img build-iso all
