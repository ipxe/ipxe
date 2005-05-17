UCLIBC_VERSION = 0.9.11
UCLIBC = uClibc-$(UCLIBC_VERSION)
$(UCLIBC)_SOURCE = http://www.uclibc.org/downloads/$(UCLIBC).tar.bz2
UCLIBC_INSTALL = $(CURDIR)/uClibc

UDHCP_VERSION = 0.9.7
UDHCP = udhcp-$(UDHCP_VERSION)
$(UDHCP)_SOURCE = http://udhcp.busybox.net/source/$(UDHCP).tar.gz

BUSYBOX_VERSION = 0.60.3
BUSYBOX = busybox-$(BUSYBOX_VERSION)
$(BUSYBOX)_SOURCE = http://www.busybox.net/downloads/$(BUSYBOX).tar.bz2

LINUX_WLAN_VERSION = 0.1.13
LINUX_WLAN = linux-wlan-ng-$(LINUX_WLAN_VERSION)
$(LINUX_WLAN)_SOURCE = ftp://ftp.linux-wlan.org/pub/linux-wlan-ng/$(LINUX_WLAN).tar.gz

MKNBI_VERSION = 1.2
MKNBI = mknbi-$(MKNBI_VERSION)
$(MKNBI)_SOURCE = http://belnet.dl.sourceforge.net/sourceforge/etherboot/$(MKNBI).tar.gz

export PATH := $(UCLIBC_INSTALL)/bin:$(PATH)

all : utils initrd-skel mknbi mknbi-linux
	# Run "make tftpboot/initrd-kernel_module.img" to generate a suitable initrd
	# Run "make tftpboot/boot-kernel_module.nbi" to generate a suitable NBI
	# Run "make all-nbi" to generate a complete set of NBIs

%.tar.bz2 :
	[ -d $* ] || wget $($*_SOURCE)
	[ -f $*.t*gz ] && ( gunzip $*.t*gz ; bzip2 -9 $*.tar ) || true

UTILS = udhcpc busybox wlanctl

utils : $(UTILS)

clean : partlyclean
	rm -rf uClibc
	rm -rf $(UCLIBC)
	rm -rf tftpboot/*

partlyclean :
	rm -rf $(UDHCP)
	rm -rf $(BUSYBOX)
	rm -rf $(LINUX_WLAN)
	rm -rf $(MKNBI)
	rm -rf initrd-skel
	rm -f *.img *.ird *.nbi insert-modules
	rm -f $(UTILS) mknbi-linux
	rm -f *.uClibc *.busybox *.udhcpc *.wlanctl

.PHONY : all utils clean partlyclean

uClibc : $(UCLIBC)
	rm -rf $@
	$(MAKE) -C $(UCLIBC) install

$(UCLIBC) : $(UCLIBC).tar.bz2
	[ -d $@ ] || tar xvjf $<
	[ -f $(UCLIBC)/Config ] || perl -pe 's/^(INCLUDE_RPC).*/$$1 = true/ ;' \
		-e 's{^(DEVEL_PREFIX).*}{$$1 = $(UCLIBC_INSTALL)} ;' \
		-e 's{^(SHARED_LIB_LOADER_PATH).*}{$$1 = /lib} ;' \
		$(UCLIBC)/extra/Configs/Config.i386 > $(UCLIBC)/Config
	# Stripping out spurious CVS directories (screws up local cvs update)
	rm -rf `find $(UCLIBC) -name CVS`
	$(MAKE) -C $(UCLIBC)
	install -m 644 $(UCLIBC)/COPYING.LIB COPYING.uClibc

udhcpc : $(UDHCP)
	install -m 755 -s $(UDHCP)/$@ $@

$(UDHCP) : $(UDHCP).tar.bz2 uClibc
	[ -d $@ ] || tar xvjf $<
	if [ ! -f $@/.script.c.patch ]; then \
		patch -d $@ -b -z .orig < script.c.patch ; \
		touch $@/.script.c.patch ; \
	fi
	$(MAKE) LDFLAGS+=-static -C $(UDHCP)
	install -m 644 $(UDHCP)/AUTHORS AUTHORS.udhcpc
	install -m 644 $(UDHCP)/COPYING COPYING.udhcpc

busybox : $(BUSYBOX)
	install -m 755 -s $(BUSYBOX)/$@ $@

$(BUSYBOX) : $(BUSYBOX).tar.bz2 uClibc
	[ -d $@ ] || tar xvjf $<
	perl -pi.orig -e \
	  's/^.*(#define BB_(FEATURE_NFSMOUNT|INSMOD|PIVOT_ROOT|IFCONFIG|ROUTE)).*/$$1/' \
	  $(BUSYBOX)/Config.h
	perl -pi.orig -e \
	  's/^(DOSTATIC).*$$/$$1 = true/' \
	  $(BUSYBOX)/Makefile
	$(MAKE) -C $(BUSYBOX)
	install -m 644 $(BUSYBOX)/AUTHORS AUTHORS.busybox
	install -m 644 $(BUSYBOX)/LICENSE LICENSE.busybox

wlanctl : $(LINUX_WLAN)
	install -m 755 -s $(LINUX_WLAN)/src/wlanctl/$@ $@

$(LINUX_WLAN) : $(LINUX_WLAN).tar.bz2 uClibc linux-wlan.cfg
	[ -d $@ ] || tar xvjf $<
	cd $(LINUX_WLAN) ; ./Configure -d ../linux-wlan.cfg
	perl -pi.orig -e \
	  's/(-o wlanctl)/-static $$1/' \
	  $(LINUX_WLAN)/src/wlanctl/Makefile
	$(MAKE) -C $(LINUX_WLAN)/src/wlanctl
	install -m 644 $(LINUX_WLAN)/COPYING COPYING.wlanctl
	install -m 644 $(LINUX_WLAN)/LICENSE LICENSE.wlanctl
	install -m 644 $(LINUX_WLAN)/THANKS THANKS.wlanctl

mknbi-linux : $(MKNBI)

mknbi : $(MKNBI)

$(MKNBI) : $(MKNBI).tar.bz2
	[ -d $@ ] || tar xvjf $<
	if [ ! -f $@/.mknbi-encap.patch ]; then \
		patch -d $@ -b -z .orig < mknbi-encap.patch ; \
		touch $@/.mknbi-encap.patch ; \
	fi
	make -C $(MKNBI) LIBDIR=`pwd`/$(MKNBI) mknbi
	install -m 755 $(MKNBI)/mknbi mknbi-linux
	make -C $(MKNBI) clean
	make -C $(MKNBI)

initrd-skel : $(UTILS) linuxrc udhcpc-post include-modules
	rm -rf $@
	mkdir -p $@
	mkdir -p $@/dev
	mkdir -p $@/etc
	mkdir -p $@/bin
	mkdir -p $@/lib
	mkdir -p $@/lib/modules
	mkdir -p $@/proc
	mkdir -p $@/sysroot
	ln -s bin $@/sbin
	install -m 755 busybox $@/bin/
	install -m 755 udhcpc $@/bin/
	install -m 755 wlanctl $@/bin/
	ln -s busybox $@/bin/sh
	ln -s busybox $@/bin/echo
	ln -s busybox $@/bin/mknod
	ln -s busybox $@/bin/chmod
	ln -s busybox $@/bin/insmod
	ln -s busybox $@/bin/ifconfig
	ln -s busybox $@/bin/route
	ln -s busybox $@/bin/mount
	ln -s busybox $@/bin/pivot_root
	ln -s busybox $@/bin/umount
	ln -s busybox $@/bin/[
	ln -s busybox $@/bin/sleep
	ln -s busybox $@/bin/grep

	install -m 755 linuxrc $@/linuxrc
	install -m 755 udhcpc-post $@/bin/udhcpc-post

tftpboot/initrd-%.img : initrd-skel
	./mkinitrd-net -l `echo $* | tr . " "`

tftpboot/boot-%.nbi : tftpboot/initrd-%.img mknbi-linux
	./mknbi-linux --format=nbi --target=linux /boot/vmlinuz $< > $@
	sudo cp $@ $(tftpbootdir)

all-nbi : all
	./mknbi-set -l -v
	ls tftpboot

prefix = /usr
sysconfdir = /etc
bindir = $(prefix)/bin
libdir = $(prefix)/lib
mandir = $(prefix)/share/man
docdir = $(prefix)/share/doc
tftpbootdir = /var/lib/tftpboot
initrdskeldir = $(prefix)/lib/mkinitrd-net/initrd-skel

install :
	mkdir -p $(libdir)/mknbi
	mkdir -p $(bindir)
	mkdir -p $(sysconfdir)
	mkdir -p $(tftpbootdir)
	mkdir -p $(initrdskeldir)
	install -m 755 mkinitrd-net include-modules mknbi-set $(bindir)/
	cp -a initrd-skel/* $(initrdskeldir)/
	install -m 644 mknbi-set.conf dhcpd.conf.etherboot.include $(sysconfdir)
	make -C $(MKNBI) INSTPREFIX=$(prefix) MANDIR=$(mandir)/man1 \
		DOCDIR=$(docdir)/$(MKNBI) install
