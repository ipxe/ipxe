# Copyright 2020 Hewlett Packard Enterprise Development LP
Name: metal-ipxe
License: GPLv2
Summary: iPXE for booting bare metal in HPCaaS environments
BuildArchitectures: noarch
Version: %(cat .version)
Release: %(echo ${BUILD_METADATA})
Source: %{name}-%{version}.tar.bz2
Vendor: Cray Inc.
BuildRequires: binutils
BuildRequires: gcc
BuildRequires: make
BuildRequires: libgcc_s1
BuildRequires: xz-devel

# The "-sb" denotes SecureBoot signing capability during compile-time.
%define binx86_64 bin-x86_64-efi-sb/ipxe.efi
%define ipxesrc ipxe/src
%define srcscript chainload.ipxe
%define wwwbootdir /var/www/boot

%description

%prep
%setup -q

%build
./toggle_ipxe_features.sh
pushd %{ipxesrc}

# Compile ipxe and embed our script.
make %{binx86_64} EMBED=%{srcscript}

%install
cp -pv %{ipxesrc}/%{binx86_64} boot/ipxe.efi

install -m 755 -d %{buildroot}%{wwwbootdir}
cp -pvrR boot/* %{buildroot}%{wwwbootdir}/ | awk '{print $3}' | sed "s/'//g" | sed "s|$RPM_BUILD_ROOT||g" | tee -a INSTALLED_FILES
cat INSTALLED_FILES | xargs -i sh -c 'test -L {} && exit || test -f $RPM_BUILD_ROOT/{} && echo {} || echo %dir {}' > INSTALLED_FILES_2

%clean
rm -f %{ipxesrc}/%{binx86_64}
rm -f %{ipxesrc}/chainload.ipxe
rm -f boot/ipxe.efi

%files -f INSTALLED_FILES_2
%defattr(-,root,root)
%license LICENSE
%doc README.md
%config(noreplace) %{wwwbootdir}/script.ipxe

%changelog
