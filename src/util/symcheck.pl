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

# Build up requires, provides and shares symbol tables for global
# symbols
#
my $globals = {};
while ( ( my $object, my $symbols ) = each %$symtab ) {
  while ( ( my $symbol, my $info ) = each %$symbols ) {
    if ( $info->{global} ) {
      my $category = ( ( $info->{type} eq 'U' ? "requires" :
			 ( $info->{type} eq 'C' ? "shares" : "provides" ) ) );
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
  my @shares = keys %{$info->{shares}};

  if ( ( @provides == 0 ) && ( @shares == 1 ) ) {
    # A symbol "shared" by just a single file is actually being
    # provided by that file; it just doesn't have an initialiser.
    @provides = @shares;
    @shares = ();
  }

  if ( ( @requires > 0 ) && ( @provides == 0 ) ) {
    # No object provides this symbol, but some objects require it.
    $problems->{$_}->{nonexistent}->{$symbol} = 1 foreach @requires;
  }

  if ( ( @requires == 0 ) && ( @provides > 0 ) ) {
    # No object requires this symbol, but some objects provide it.
    $problems->{$_}->{unused}->{$symbol} = 1 foreach @provides;
  }

  if ( ( @shares > 0 ) && ( @requires > 0 ) ) {
    # A shared symbol is being referenced from another object
    $problems->{$_}->{shared}->{$symbol} = 1 foreach @requires;
  }

  if ( ( @shares > 0 ) && ( @provides > 0 ) ) {
    # A shared symbol is being initialised by an object
    $problems->{$_}->{shared}->{$symbol} = 1 foreach @provides;
  }

  if ( ( @shares > 0 ) && ! ( $symbol =~ /^_shared_/ ) ) {
    # A shared symbol is not declared via __shared
    $problems->{$_}->{shared}->{$symbol} = 1 foreach @shares;
  }

  if ( @provides > 1 ) {
    # A non-shared symbol is defined in multiple objects
    $problems->{$_}->{multiples}->{$symbol} = 1 foreach @provides;
  }
}

# Check for excessively large local symbols
#
while ( ( my $object, my $symbols ) = each %$symtab ) {
  while ( ( my $symbol, my $info ) = each %$symbols ) {
    if ( ( ! $info->{global} ) &&
	 ( $info->{type} ne 't' ) &&
	 ( $info->{size} >= WARNING_SIZE ) ) {
      $problems->{$object}->{large}->{$symbol} = 1;
    }
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
  my @shared = sort keys %{$problems->{$object}->{shared}};
  my @large = sort keys %{$problems->{$object}->{large}};

  print "WARN $object provides unused symbol $_\n" foreach @unused;
  $warnings += @unused;
  print "WARN $object has large static symbol $_\n" foreach @large;
  $warnings += @large;
  print "ERR  $object requires non-existent symbol $_\n" foreach @nonexistent;
  $errors += @nonexistent;
  foreach my $symbol ( @multiples ) {
    my @other_objects = sort grep { $_ ne $object }
		        keys %{$globals->{$symbol}->{provides}};
    print "ERR  $object provides symbol $symbol"
	." (also provided by @other_objects)\n";
  }
  $errors += @multiples;
  print "ERR  $object misuses shared symbol $_\n" foreach @shared;
}

print "$errors error(s), $warnings warning(s)\n";
exit ( $errors ? 1 : 0 );
