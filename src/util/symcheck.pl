#!/usr/bin/perl -w

use strict;
use warnings;

my $symbols = {};

# Scan output of "nm -o -g bin/blib.a" and build up symbol cross-ref table
#
while ( <> ) {
  chomp;
  ( my $object, my $type, my $symbol ) = /^.*?:(.*?\.o):.*?\s(\S)\s(.*)$/;
  my $category = $type eq 'U' ? "requires" : "provides";
  $symbols->{$symbol}->{$category}->{$object} = 1;
}

# Add symbols that we know will be generated or required by the linker
#
while ( ( my $symbol, my $info ) = each %$symbols ) {
  $info->{requires}->{LINKER} = 1 if $symbol =~ /^obj_/;
}
$symbols->{$_}->{provides}->{LINKER} = 1
    foreach qw ( _prefix _eprefix _decompress _edecompress _text
		 _etext _data _edata _bss _ebss _end device_drivers
		 device_drivers_end bus_drivers bus_drivers_end
		 type_drivers type_drivers_end console_drivers
		 console_drivers_end post_reloc_fns post_reloc_fns_end
		 init_fns init_fns_end );

# Check for multiply defined, never-defined and unused symbols
#
my $problems = {};
while ( ( my $symbol, my $info ) = each %$symbols ) {
  my @provides = keys %{$info->{provides}};
  my @requires = keys %{$info->{requires}};

  if ( @provides == 0 ) {
    # No object provides this symbol
    $problems->{$_}->{nonexistent}->{$symbol} = 1 foreach @requires;
  } elsif ( @provides > 1 ) {
    # Symbol defined in multiple objects
    $problems->{$_}->{multiples}->{$symbol} = 1 foreach @provides;
  }
  if ( @requires == 0 ) {
    # Symbol not required
    $problems->{$_}->{unused}->{$symbol} = 1 foreach @provides;
  }
}

# Print out error messages
#
my $errors = 0;
my $warnings = 0;
foreach my $object ( sort keys %$problems ) {
  my @nonexistent = sort keys %{$problems->{$object}->{nonexistent}};
  my @multiples = sort keys %{$problems->{$object}->{multiples}};
  my @unused = sort keys %{$problems->{$object}->{unused}};

  print "WARN $object provides unused symbol $_\n" foreach @unused;
  $warnings += @unused;
  print "ERR  $object requires non-existent symbol $_\n" foreach @nonexistent;
  $errors += @nonexistent;
  foreach my $symbol ( @multiples ) {
    my @other_objects = sort grep { $_ ne $object }
		        keys %{$symbols->{$symbol}->{provides}};
    print "ERR  $object provides symbol $symbol"
	." (also provided by @other_objects)\n";
  }
  $errors += @multiples;
}

print "$errors error(s), $warnings warning(s)\n";
exit ( $errors ? 1 : 0 );
