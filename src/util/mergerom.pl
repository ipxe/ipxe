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

my @romfiles = @ARGV;
my @roms = map { my $rom = new Option::ROM; $rom->load($_); $rom } @romfiles;

my $baserom = shift @roms;
my $offset = $baserom->length;

foreach my $rom ( @roms ) {

  # Update base length
  $baserom->{length} += $rom->{length};

  # Update PCI header, if present in both
  my $baserom_pci = $baserom->pci_header;
  my $rom_pci = $rom->pci_header;
  if ( $baserom_pci && $rom_pci ) {

    # Update PCI lengths
    $baserom_pci->{image_length} += $rom_pci->{image_length};
    if ( exists $baserom_pci->{runtime_length} ) {
      if ( exists $rom_pci->{runtime_length} ) {
	$baserom_pci->{runtime_length} += $rom_pci->{runtime_length};
      } else {
	$baserom_pci->{runtime_length} += $rom_pci->{image_length};
      }
    }

    # Merge CLP entry point
    if ( exists ( $baserom_pci->{clp_entry} ) &&
	 exists ( $rom_pci->{clp_entry} ) ) {
      $baserom_pci->{clp_entry} = ( $offset + $rom_pci->{clp_entry} )
	  if $rom_pci->{clp_entry};
    }
  }

  # Update PnP header, if present in both
  my $baserom_pnp = $baserom->pnp_header;
  my $rom_pnp = $rom->pnp_header;
  if ( $baserom_pnp && $rom_pnp ) {
    $baserom_pnp->{bcv} = ( $offset + $rom_pnp->{bcv} ) if $rom_pnp->{bcv};
    $baserom_pnp->{bdv} = ( $offset + $rom_pnp->{bdv} ) if $rom_pnp->{bdv};
    $baserom_pnp->{bev} = ( $offset + $rom_pnp->{bev} ) if $rom_pnp->{bev};
  }

  # Fix checksum for this ROM segment
  $rom->fix_checksum();

  $offset += $rom->length;
}

$baserom->pnp_header->fix_checksum() if $baserom->pnp_header;
$baserom->fix_checksum();
$baserom->save ( "-" );
foreach my $rom ( @roms ) {
  $rom->save ( "-" );
}
