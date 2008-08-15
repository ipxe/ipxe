#!/usr/bin/perl -w
#
# Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin";
use Option::ROM qw ( :all );

my $romfile = shift || "-";
my $rom = new Option::ROM;
$rom->load ( $romfile );

die "Not an option ROM image\n"
    unless $rom->{signature} == ROM_SIGNATURE;

my $romlength = ( $rom->{length} * 512 );
my $filelength = $rom->length;
die "ROM image truncated (is $filelength, should be $romlength)\n"
    if $filelength < $romlength;

printf "ROM header:\n\n";
printf "  Length:\t0x%02x (%d)\n", $rom->{length}, ( $rom->{length} * 512 );
printf "  Checksum:\t0x%02x (0x%02x)\n", $rom->{checksum}, $rom->checksum;
printf "  Init:\t\t0x%04x\n", $rom->{init};
printf "  UNDI header:\t0x%04x\n", $rom->{undi_header};
printf "  PCI header:\t0x%04x\n", $rom->{pci_header};
printf "  PnP header:\t0x%04x\n", $rom->{pnp_header};
printf "\n";

my $pci = $rom->pci_header();
if ( $pci ) {
  printf "PCI header:\n\n";
  printf "  Signature:\t%s\n", $pci->{signature};
  printf "  Vendor id:\t0x%04x\n", $pci->{vendor_id};
  printf "  Device id:\t0x%04x\n", $pci->{device_id};
  printf "  Device class:\t0x%02x%02x%02x\n",
	 $pci->{base_class}, $pci->{sub_class}, $pci->{prog_intf};
  printf "  Image length:\t0x%04x (%d)\n",
	 $pci->{image_length}, ( $pci->{image_length} * 512 );
  printf "  Runtime length:\t0x%04x (%d)\n",
	 $pci->{runtime_length}, ( $pci->{runtime_length} * 512 );
  printf "  Config header:\t0x%04x\n", $pci->{conf_header};
  printf "  CLP entry:\t0x%04x\n", $pci->{clp_entry};
  printf "\n";
}

my $pnp = $rom->pnp_header();
if ( $pnp ) {
  printf "PnP header:\n\n";
  printf "  Signature:\t%s\n", $pnp->{signature};
  printf "  Checksum:\t0x%02x (0x%02x)\n", $pnp->{checksum}, $pnp->checksum;
  printf "  Manufacturer:\t0x%04x \"%s\"\n",
	 $pnp->{manufacturer}, $pnp->manufacturer;
  printf "  Product:\t0x%04x \"%s\"\n", $pnp->{product}, $pnp->product;
  printf "  BCV:\t\t0x%04x\n", $pnp->{bcv};
  printf "  BDV:\t\t0x%04x\n", $pnp->{bdv};
  printf "  BEV:\t\t0x%04x\n", $pnp->{bev};
  printf "\n";
}
