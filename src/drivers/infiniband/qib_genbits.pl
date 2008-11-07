#!/usr/bin/perl -w

use strict;
use warnings;

my $offsets = {};
my $structures = {};
my $structure = "";

while ( <> ) {
  chomp;
  if ( /^\#define (\S+)_OFFS (\S+)$/ ) {
    $structure = $1;
    $offsets->{$structure} = $2;
  } elsif ( /^\#define ${structure}_(\S+)_LSB (\S+)$/ ) {
    $structures->{$structure}->{$1}->{LSB} = $2;
  } elsif ( /^\#define ${structure}_(\S+)_RMASK (\S+)$/ ) {
    $structures->{$structure}->{$1}->{RMASK} = $2;
  } elsif ( /^\s*$/ ) {
    # Do nothing
  } else {
    print "$_\n";
  }
}

my $data = [ map { { name => $_, offset => $offsets->{$_} }; }
	     sort { hex ( $offsets->{$a} ) <=> hex ( $offsets->{$b} ) }
	     keys %$offsets ];

foreach my $datum ( @$data ) {
  next unless exists $structures->{$datum->{name}};
  $structure = $structures->{$datum->{name}};
  my $fields = [ map { { name => $_, lsb => $structure->{$_}->{LSB},
			 rmask => $structure->{$_}->{RMASK} }; }
		 sort { hex ( $structure->{$a}->{LSB} ) <=>
			    hex ( $structure->{$b}->{LSB} ) }
		 keys %$structure ];
  $datum->{fields} = $fields;
}

print "\n/* This file has been further processed by $0 */\n\n\n";

foreach my $datum ( @$data ) {
  printf "#define %s_offset 0x%08xUL\n",
      $datum->{name}, hex ( $datum->{offset} );
  if ( exists $datum->{fields} ) {
    my $lsb = 0;
    my $reserved_idx = 0;
    printf "struct %s_pb {\n", $datum->{name};
    foreach my $field ( @{$datum->{fields}} ) {
      my $pad_width = ( hex ( $field->{lsb} ) - $lsb );
      die "Inconsistent LSB/RMASK in $datum->{name}\n" if $pad_width < 0;
      printf "\tpseudo_bit_t _unused_%u[%u];\n", $reserved_idx++, $pad_width
	  if $pad_width;
      # Damn Perl can't cope with 64-bit hex constants
      my $width = 0;
      my $rmask = $field->{rmask};
      while ( $rmask =~ /^(0x.+)f$/i ) {
	$width += 4;
	$rmask = $1;
      }
      $rmask = hex ( $rmask );
      while ( $rmask ) {
	$width++;
	$rmask >>= 1;
      }
      printf "\tpseudo_bit_t %s[%u];\n", $field->{name}, $width;
      $lsb += $width;
    }
    my $pad_width = ( 64 - $lsb );
    die "Inconsistent LSB/RMASK in $datum->{name}\n" if $pad_width < 0;
    printf "\tpseudo_bit_t _unused_%u[%u];\n", $reserved_idx++, $pad_width
	if $pad_width;
    printf "};\n";
    printf "struct %s {\n\tPSEUDO_BIT_STRUCT ( struct %s_pb );\n};\n",
	$datum->{name}, $datum->{name};
  }
  print "\n";
}
