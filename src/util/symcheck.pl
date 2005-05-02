#!/usr/bin/perl -w

use strict;
use warnings;

use constant WARNING_SIZE => 2048;

my $symtab = {};

# Scan output of "nm -o -S bin/blib.a" and build up symbol table
#
while ( <> ) {
  chomp;
  ( my $object, undef, my $value, undef, my $size, my $type, my $symbol )
      = /^.*?:(.*?\.o):((\S+)(\s+(\S+))?)?\s+(\S)\s+(\S+)$/;
  $symtab->{$object}->{$symbol} = {
    global	=> ( $type eq uc $type ),
    type	=> ( $type ),
    value	=> ( $value ? hex ( $value ) : 0 ),
    size	=> ( $size ? hex ( $size ) : 0 ),
  };
}

# Add symbols that we know will be generated or required by the linker
#
foreach my $object ( keys %$symtab ) {
  my $obj_symbol = "obj_$object";
  $obj_symbol =~ s/\.o$//;
  $obj_symbol =~ s/\W/_/g;
  $symtab->{LINKER}->{$obj_symbol} = {
    global	=> 1,
    type	=> 'U',
    value	=> 0,
    size	=> 0,
  };
}
foreach my $link_sym qw ( _prefix _eprefix _decompress _edecompress _text
			  _etext _data _edata _bss _ebss _end ) {
  $symtab->{LINKER}->{$link_sym} = {
    global	=> 1,
    type       	=> 'A',
    value	=> 0,
    size	=> 0,
  };
}

# Build up requires and provides tables for global symbols
my $globals = {};
while ( ( my $object, my $symbols ) = each %$symtab ) {
  while ( ( my $symbol, my $info ) = each %$symbols ) {
    if ( $info->{global} ) {
      my $category = ( ( $info->{type} eq 'U' ? "requires" : "provides" ) );
      $globals->{$symbol}->{$category}->{$object} = 1;
    }
  }
}

# Check for multiply defined, never-defined and unused global symbols
#
my $problems = {};
while ( ( my $symbol, my $info ) = each %$globals ) {
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
		        keys %{$globals->{$symbol}->{provides}};
    print "ERR  $object provides symbol $symbol"
	." (also provided by @other_objects)\n";
  }
  $errors += @multiples;
}

print "$errors error(s), $warnings warning(s)\n";
exit ( $errors ? 1 : 0 );
