#!/usr/bin/perl -w
#
# Program to apply an unnrv2b decompressor header to an ia64 etherboot file.
#
# GPL Eric Biederman 2002
#

use strict;

use bytes;

main(@ARGV);

sub usage
{
	my ($err) = @_;
	print STDERR $err , "\n";
	die "Usage $0 prefix file\n";
}

sub getbits
{
	my ($bundle, $start, $size) = @_;

	# Compute the mask 
	my $mask = 0xffffffff;
	$mask = $mask >> (32 - $size);

	# Compute the substring, and shift
	my ($first, $end, $count, $shift);
	$first = int($start / 8);
	$end   = int(($start + $size + 7)/8);
	$count = $end - $first;
	$shift = $start % 8;

	# Compute the unpack type
	my $type;
	if ($count == 1) {
		$type = "C"
	}
	elsif ($count == 2) {
		$type = "v";
	}
	elsif (($count >= 3) && ($count <= 4)) {
		$type = "V";
	}
	else {
		die "bad count $count";
	}

	# Now compute the value
	my $val = (unpack($type, substr($bundle, $first, $count)) >> $shift) & $mask;
	
	# Now return the value
	return $val;
}

sub putbits
{
	my ($bundle, $start, $size, $val) = @_;


	# Compute the mask 
	my $mask = 0xffffffff;
	$mask >>= 32 - $size;

	# Compute the substring, and shift
	my ($first, $end, $count, $shift);
	$first = int($start / 8);
	$end   = int(($start + $size + 7)/8);
	$count = $end - $first;
	$shift = $start % 8;

	# Compute the unpack type
	my $type;
	if ($count == 1) {
		$type = "C"
	}
	elsif ($count == 2) {
		$type = "v";
	}
	elsif (($count >= 3) && ($count <= 4)) {
		$type = "V";
	}
	else {
		die "bad count $count";
	}

	# Adjust the mask
	$mask <<= $shift;

	# Now set the value, preserving the untouched bits
	substr($bundle, $first, $count) =
	    pack($type, 
		 ((unpack($type, substr($bundle, $first, $count)) & ~$mask) |
		  (($val << $shift) & $mask)));

	# Now return the new value;
	return $bundle;
}

sub main
{
	my ($prefix_name, $suffix_name) = @_;
	usage("No prefix") unless (defined($prefix_name));
	usage("No suffix") unless (defined($suffix_name));

	open(PREFIX, "<$prefix_name") or die "Cannot open $prefix_name";
	open(SUFFIX, "<$suffix_name") or die "Cannot open $suffix_name";

	$/ = undef;
	my $prefix = <PREFIX>; close(PREFIX);
	my $suffix = <SUFFIX>; close(SUFFIX);

	# Payload sizes
	my $prefix_len = length($prefix);
	my $suffix_len = length($suffix);
	my $payload_size = $suffix_len;
	my $uncompressed_offset = ($prefix_len + $suffix_len + 15) & ~15;
	my $pad = $uncompressed_offset - ($prefix_len + $suffix_len);

	# Itaninum instruction bundle we will be updating
	#  0 -   4  template == 5
	#  5 -  45  slot 0  M-Unit 
	# 46 -  86  slot 1  L-Unit 
	# 87 - 127  slot 2  X-Unit
	# Itaninum instruction format
	# 40 - 37   Major opcode 
	# ... 
	#

	# slot 1
	#  0 - 40 [41] imm-41
	# 10 - 40 [31] imm-41-hi
	#  0 - 9  [10] imm-41-lo

	# slot 2
	#  0 -  5  [6] qp
	#  6 - 12  [7] r1
	# 13 - 19  [7] imm-7b
	# 20       [1] vc
	# 21       [1] immc
	# 22 - 26  [5] imm-5c
	# 27 - 35  [9] imm-9d
	# 36       [1] imm0
	# 37 - 40  [4] major opcode
	#
	
	# major opcode should be 6

	# Update the image size
	my $uncompressed_offset_bundle_off = 16;
	my $bundle = substr($prefix, $uncompressed_offset_bundle_off, 16);

	my $template      = getbits($bundle, 0,  5);
	my $op1_base      = 46;
	my $op2_base      = 87;
	my $major_opcode  = getbits($bundle, 37 + $op2_base, 4);

	if (($template != 5) ||
		($major_opcode != 6)) {
		die "unknown second bundle cannot patch";
	}

	die "uncompressed_offset to big!\n" if ($uncompressed_offset > 0xffffffff);
	my $immhi         = 0;
	my $immlo         = $uncompressed_offset;
	 
	my $imm0          = ($immhi >> 31) & ((1 <<  1) - 1);
	my $imm41_hi      = ($immhi >>  0) & ((1 << 31) - 1);
	 
	my $imm41_lo      = ($immlo >> 22) & ((1 << 10) - 1);
	my $immc          = ($immlo >> 21) & ((1 <<  1) - 1);
	my $imm5c         = ($immlo >> 16) & ((1 <<  5) - 1);
	my $imm9d         = ($immlo >>  7) & ((1 <<  9) - 1);
	my $imm7b         = ($immlo >>  0) & ((1 <<  7) - 1);

	$bundle           = putbits($bundle, 10 + $op1_base, 31, $imm41_hi);
	$bundle           = putbits($bundle,  0 + $op1_base, 10, $imm41_lo);
	$bundle           = putbits($bundle, 36 + $op2_base, 1 , $imm0);
	$bundle           = putbits($bundle, 27 + $op2_base, 9 , $imm9d);
	$bundle           = putbits($bundle, 22 + $op2_base, 5 , $imm5c);
	$bundle           = putbits($bundle, 21 + $op2_base, 1 , $immc);
	$bundle           = putbits($bundle, 13 + $op2_base, 7 , $imm7b);

	substr($prefix, $uncompressed_offset_bundle_off, 16) = $bundle;

	#print (STDERR "prefix:                  $prefix_len\n");
	#print (STDERR "suffix:                  $suffix_len\n");
	#print (STDERR "pad:                     $pad\n");
	#print (STDERR "uncompressed_offset:     $uncompressed_offset\n");

	print $prefix;
	print $suffix;
	# Pad the resulting image by a few extra bytes... 
	print pack("C", 0) x $pad;
}

