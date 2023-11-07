#!/usr/bin/make -f
# -*- makefile -*-

# Uncomment this to turn on verbose mode.
#export DH_VERBOSE=1

TMPDIR=$(CURDIR)/tmp

#MAKEARGS:=DEBUG="device,pci,init,nii,snp,snpnet,snponly,efi_watchdog,efi_image,image_cmd,image,efi_autoboot,efi_timer,efi_driver:3,efi_debug,efi_init,efi_pci,efi_wrap,intel" LOG_LEVEL=LOG_ALL

# parameters
# $(1) = target, $(2) = embedded file, $(3) = output file, $(4) = architecture
define make_with_name
	mkdir -p $(dir $(3)) $(TMPDIR)/$(3)
	cp -r src $(TMPDIR)/$(3)/
	+CROSS_COMPILE=$(4)-linux-gnu- $(MAKE) EMBED=$(shell basename $(2)) $(MAKEARGS) \
	  -C $(TMPDIR)/$(3)/src $(1) > /dev/null
	mv $(TMPDIR)/$(3)/src/$(1) $(3)
endef

all: build

# kpxe kernels
kernels/x86_64/ipxe-autoboot.lkrn: src/ipxe-autoboot-no-ipoib.txt
	$(call make_with_name,bin-x86_64-pcbios/ipxe--ecm--ncm.lkrn,$<,$@,x86_64)

kernels/x86_64/ipxe-shell.lkrn: src/ipxe-shell.txt
	$(call make_with_name,bin-x86_64-pcbios/ipxe--ecm--ncm.lkrn,$<,$@,x86_64)

# UEFI kernels
kernels/x86_64/ipxe-autoboot.efi: src/ipxe-autoboot-no-ipoib.txt
	$(call make_with_name,bin-x86_64-efi/ipxe--ecm--ncm.efi,$<,$@,x86_64)
kernels/x86_64/ipxe-shell.efi: src/ipxe-shell.txt
	$(call make_with_name,bin-x86_64-efi/ipxe--ecm--ncm.efi,$<,$@,x86_64)

# Arm64 UEFI kernels
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

build-arm64: binaries/arm64/ipxe-autoboot.iso binaries/arm64/ipxe-shell.iso

build-img: binaries/x86_64/ipxe-autoboot.img binaries/x86_64/ipxe-shell.img
build-iso: binaries/x86_64/ipxe-autoboot.iso binaries/x86_64/ipxe-shell.iso build-arm64
build: build-img build-iso

.phony: clean build build-arm64 build-img build-iso all
