# Copyright 2020 Hewlett Packard Enterprise Development LP
Name: metal-ipxe
License: GPLv2
Summary: iPXE for booting bare metal in HPCaaS environments
BuildArch: noarch
Version: %(cat .version)
Release: %(echo ${BUILD_METADATA})
Source: %{name}-%{version}.tar.bz2
Vendor: Cray Inc.
BuildRequires: binutils
BuildRequires: gcc
BuildRequires: make
BuildRequires: libgcc_s1
BuildRequires: xz-devel
Requires: dnsmasq

# The "-sb" denotes SecureBoot signing capability during compile-time.
%define binx86_64 bin-x86_64-efi-sb/ipxe.efi
%define ipxesrc src/
%define srcscript config/metal/metal.ipxe
%define bootscript script.ipxe
%define wwwbootdir /var/www/boot/

%description

%prep
%setup -q

%build
pushd %{ipxesrc}
make -j 4 %{binx86_64} EMBED=%{srcscript} CONFIG=metal

%install
install -m 755 -d %{buildroot}%{wwwbootdir}
install -m 644 %{ipxesrc}%{binx86_64} %{buildroot}%{wwwbootdir}
install -m 644 %{bootscript} %{buildroot}%{wwwbootdir}

%clean
rm -f %{ipxesrc}%{binx86_64}
rm -f %{ipxesrc}%{srcscript}

%files
%defattr(-,root,root)
%license COPYING.GPLv2
%doc README.asc
%config(noreplace) %{wwwbootdir}%{bootscript}
%attr(-,dnsmasq,tftp) %{wwwbootdir}%(basename %{binx86_64})

%changelog
