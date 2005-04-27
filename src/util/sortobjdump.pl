#!/usr/bin/perl -w

use strict;
use warnings;

# Sort the symbol table portion of the output of objdump -ht by
# section, then by symbol value.  Used to enhance the linker maps
# produced by "make bin/%.map" by also showing the values of all
# non-global symbols.

my %section_idx = ( "*ABS*" => "." );
my %lines;
while ( <> ) {
  if ( /^\s+(\d+)\s+([\.\*]\S+)\s+[0-9a-fA-F]+\s+[0-9a-fA-F]/ ) {
    # It's a header line containing a section definition; extract the
    # section index and store it.  Also print the header line.
    print;
    ( my $index, my $section ) = ( $1, $2 );
    $section_idx{$section} = sprintf ( "%02d", $index );
  } elsif ( /^([0-9a-fA-F]+)\s.*?\s([\.\*]\S+)\s/ ) {
    # It's a symbol line - store it in the hash, indexed by
    # "<section index>.<value>"
    ( my $value, my $section ) = ( $1, $2 );
    die "Unrecognised section \"$section\"\n"
	unless exists $section_idx{$section};
    my $section_idx = $section_idx{$section};
    $lines{${section_idx}.":".${value}} = $_;
  } else {
    # It's a generic header line: just print it.
    print;
  }
}

print $lines{$_} foreach sort keys %lines;
