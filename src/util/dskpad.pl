#!/usr/bin/perl -w

use strict;
use warnings;

use constant FLOPPYSIZE => 1440 * 1024;

while ( my $filename = shift ) {
  die "$filename is not a file\n" unless -f $filename;
  die "$filename is too large\n" unless ( -s $filename <= FLOPPYSIZE );
  truncate $filename, FLOPPYSIZE or die "Could not truncate: $!\n";
}
