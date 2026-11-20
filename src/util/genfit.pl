#!/usr/bin/perl -w
#
# Copyright (C) 2026 Bejean Mosher <bejean.mosher@intel.com>.
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

=head1 NAME

genfit.pl

=head1 SYNOPSIS

genfit.pl [options] <flat binary> <FIT image>

Options:

    -l,--load=ADDR	Physical address at which the FIT image is loaded
    -a,--arch=ARCH	Architecture name (default "x86_64")
    -h,--help		Display brief help message
    -v,--verbose	Increase verbosity
    -q,--quiet		Decrease verbosity

=head1 DESCRIPTION

Wrap a flat binary within a Flattened Image Tree (FIT) as used for
Universal Payloads.  The binary is executed in place at the load
address plus its offset within the FIT image, with the entry point at
the start of the binary.

=cut

use strict;
use warnings;
use Getopt::Long;
use Pod::Usage;

use constant FDT_MAGIC => 0xd00dfeed;
use constant FDT_VERSION => 17;
use constant FDT_LAST_COMP_VERSION => 16;
use constant FDT_BEGIN_NODE => 1;
use constant FDT_END_NODE => 2;
use constant FDT_PROP => 3;
use constant FDT_END => 9;
use constant FDT_HEADER_LEN => 40;
use constant FDT_RSVMAP_LEN => 16;
use constant DATA_ALIGN => 4096;
use constant RELOC_ENTRY_LEN => 16;

my $load;
my $arch = "x86_64";
my $verbosity = 0;

Getopt::Long::Configure ( "bundling", "auto_abbrev" );
GetOptions (
  "load|l=o" => \$load,
  "arch|a=s" => \$arch,
  "help|h" => sub { pod2usage ( 1 ); },
  "verbose|v+" => sub { $verbosity++; },
  "quiet|q+" => sub { $verbosity--; },
) or die "Could not parse command-line options\n";
pod2usage ( 1 ) unless @ARGV == 2;
die "Load address must be specified\n" unless defined $load;
my $infile = shift;
my $outfile = shift;

# Read flat binary, padded to allow for an aligned relocation table
open my $in, "<", $infile or die "Could not open $infile: $!\n";
binmode $in;
my $binary = do { local $/; <$in> };
close $in;
$binary .= "\0" x ( ( -length $binary ) % 8 );

# Construct device tree with the specified data offset
sub fdt {
  my $data_offset = shift;
  my $struct = "";
  my $strings = "";
  my %stroff;

  my $pad = sub {
    my $data = shift;
    return $data . ( "\0" x ( ( -length $data ) % 4 ) );
  };
  my $begin = sub {
    $struct .= ( pack ( "N", FDT_BEGIN_NODE ) . $pad->( shift . "\0" ) );
  };
  my $end = sub { $struct .= pack ( "N", FDT_END_NODE ); };
  my $prop = sub {
    my $name = shift;
    my $value = shift;
    unless ( exists $stroff{$name} ) {
      $stroff{$name} = length $strings;
      $strings .= $name . "\0";
    }
    $struct .= ( pack ( "NNN", FDT_PROP, length $value, $stroff{$name} ) .
		 $pad->( $value ) );
  };
  my $str = sub { $prop->( shift, shift . "\0" ); };
  my $u32 = sub { $prop->( shift, pack ( "N", shift ) ); };
  my $u64 = sub {
    my $name = shift;
    my $value = shift;
    $prop->( $name, pack ( "NN", ( $value >> 32 ), ( $value & 0xffffffff ) ) );
  };

  # A single (ignored) relocation entry, since the relocation table
  # must lie within the image
  my $data_size = ( length ( $binary ) + RELOC_ENTRY_LEN );
  my $reloc_start = ( $data_offset + length ( $binary ) );
  my $size = ( $data_offset + $data_size );

  $begin->( "" );
  $str->( "description", "iPXE" );
  $u32->( "size", $size );
  $begin->( "images" );
  $begin->( "ipxe" );
  $str->( "description", "iPXE" );
  $str->( "type", "firmware" );
  $str->( "arch", $arch );
  $str->( "compression", "none" );
  $u32->( "data-offset", $data_offset );
  $u32->( "data-size", $data_size );
  $u32->( "reloc-start", $reloc_start );
  $u64->( "load", $load );
  $u64->( "entry-start", ( $load + $data_offset ) );
  $end->();
  $end->();
  $begin->( "configurations" );
  $str->( "default", "conf-1" );
  $begin->( "conf-1" );
  $str->( "firmware", "ipxe" );
  $end->();
  $end->();
  $end->();
  $struct .= pack ( "N", FDT_END );

  my $off_rsvmap = FDT_HEADER_LEN;
  my $off_struct = ( $off_rsvmap + FDT_RSVMAP_LEN );
  my $off_strings = ( $off_struct + length $struct );
  my $total = ( $off_strings + length $strings );
  return ( pack ( "N10", FDT_MAGIC, $total, $off_struct, $off_strings,
		  $off_rsvmap, FDT_VERSION, FDT_LAST_COMP_VERSION, 0,
		  length $strings, length $struct ) .
	   ( "\0" x FDT_RSVMAP_LEN ) . $struct . $strings );
}

# Construct FIT image (the device tree size is independent of the
# data offset, so a second pass is sufficient)
my $fdt = fdt ( 0 );
my $data_offset = ( length ( $fdt ) + ( ( -length $fdt ) % DATA_ALIGN ) );
$fdt = fdt ( $data_offset );
my $fit = ( $fdt . ( "\0" x ( $data_offset - length $fdt ) ) . $binary .
	    ( "\0" x RELOC_ENTRY_LEN ) );

open my $out, ">", $outfile or die "Could not open $outfile: $!\n";
binmode $out;
print $out $fit;
close $out;

printf "FIT image %s loads at %#x with entry point %#x\n",
       $outfile, $load, ( $load + $data_offset ) if $verbosity >= 1;
