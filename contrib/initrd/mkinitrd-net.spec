%define name mkinitrd-net
%define version 1.10
%define release 1fs

Summary: Network-booting initrd builder 
Name: %{name}
Version: %{version}
Release: %{release}
Source0: %{name}-%{version}.tar.bz2
Source1: http://belnet.dl.sourceforge.net/sourceforge/etherboot/mknbi-1.2.tar.bz2
Source2: http://www.busybox.net/downloads/busybox-0.60.3.tar.bz2
Source3: http://www.uclibc.org/downloads/uClibc-0.9.11.tar.bz2
Source4: ftp://ftp.linux-wlan.org/pub/linux-wlan-ng/linux-wlan-ng-0.1.13.tar.bz2
Source5: http://udhcp.busybox.net/source/udhcp-0.9.7.tar.bz2
Copyright: GPL/LGPL/MPL
Group: System/Kernel and hardware
BuildRoot: %{_tmppath}/%{name}-buildroot
Prefix: %{_prefix}
Requires: tftp-server

%description
mkinitrd-net allows you to build initial ramdisk images (initrds) suitable
for use with Etherboot and other network-booting software.  This package
contains two main utilities: mkinitrd-net (to build an initrd containing a
specified set of network-card modules) and mknbi (to generate
Etherboot-usable NBI images from a given kernel and initrd).  It also
contains a helper script mknbi-set which will maintain sets of initrds to
match all your currently-installed kernels.

mkinitrd-net uses code from the uClibc, busybox, udhcp and Etherboot
projects.

%prep
%setup -n initrd -a1 -a2 -a3 -a4 -a5

%build
%make LIBDIR=%{_libdir}/mknbi

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall tftpbootdir=$RPM_BUILD_ROOT%{_localstatedir}/tftpboot
touch $RPM_BUILD_ROOT%{_sysconfdir}/dhcpd.conf.etherboot-pcimap.include
ln -s %{_localstatedir}/tftpboot $RPM_BUILD_ROOT/tftpboot

%clean
rm -rf $RPM_BUILD_ROOT

%post
%{_bindir}/mknbi-set

%triggerin -- kernel kernel-smp kernel-secure kernel-enterprise
%{_bindir}/mknbi-set

%files
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/mknbi-set.conf
%config(noreplace) %{_sysconfdir}/dhcpd.conf.etherboot.include
%ghost %{_sysconfdir}/dhcpd.conf.etherboot-pcimap.include
%{_bindir}/mknbi-*
%{_bindir}/mkelf-*
%{_bindir}/dis*
%{_bindir}/mkinitrd-net
%{_bindir}/include-modules
%{_libdir}/mknbi
%{_libdir}/mkinitrd-net
%{_mandir}/man*/*
/tftpboot
%{_localstatedir}/tftpboot
%doc README
%doc AUTHORS.busybox LICENSE.busybox
%doc AUTHORS.udhcpc COPYING.udhcpc
%doc COPYING.wlanctl LICENSE.wlanctl THANKS.wlanctl
%doc COPYING.uClibc
%docdir %{_docdir}/mknbi*
%{_docdir}/mknbi*

%changelog
* Fri Jul 26 2002 Michael Brown <mbrown@fensystems.co.uk> 1.10-1fs
- Support for new binary etherboot.nic-dev-id structure
- Added --kernel option patch from Stew Benedict at MandrakeSoft
- Only try to use sudo if we are not already root

* Wed Jun 05 2002 Michael Brown <mbrown@fensystems.co.uk> 1.9-1fs
- Modifications to allow DHCP, TFTP and NFS servers to be separate machines.

* Thu May 30 2002 Michael Brown <mbrown@fensystems.co.uk> 1.8-1fs
- /tftpboot symlinked to /var/lib/tftpboot
- Has ability to be quiet if "quiet" specified on kernel cmdline

* Sun May 26 2002 Michael Brown <mbrown@fensystems.co.uk> 1.7-1fs
- PCI-ID auto-mapping via dhcpd.conf.etherboot-pcimap.include

* Fri May 24 2002 Michael Brown <mbrown@fensystems.co.uk> 1.6-1fs
- Bugfixes, migrated /tftpboot to /var/lib/tftpboot

* Thu May 23 2002 Michael Brown <mbrown@fensystems.co.uk> 1.5-1fs
- Now includes dhcpd.conf.etherboot.include
- Automatically scans for all network modules in the pcimap file

* Wed May 08 2002 Michael Brown <mbrown@fensystems.co.uk> 1.4-1fs
- Bugfixes: tmpdir selection, linuxrc typos, ifconfig peculiarities

* Sat May 04 2002 Michael Brown <mbrown@fensystems.co.uk> 1.3-1fs
- During %make, LIBDIR must be set for mknbi
- Added %post scriptlet since %trigger seems not to be being triggered...

* Sat May 04 2002 Michael Brown <mbrown@fensystems.co.uk> 1.2-1fs
- Added extra sources instead of requiring "make" to download them

* Sat May 04 2002 Michael Brown <mbrown@fensystems.co.uk> 1.1-1fs
- First attempt at an RPM package

