#!/usr/bin/perl -w
#
# Program to apply an efi header to an ia64 etherboot file.
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
	die "Usage $0 prrefix file bss_size\n";
}
sub main
{
	my ($prefix_name, $suffix_name, $bss_size) = @_;
	usage("No prefix") unless (defined($prefix_name));
	usage("No suffix") unless (defined($suffix_name));
	usage("No bss size") unless (defined($bss_size));

	open(PREFIX, "<$prefix_name") or die "Cannot open $prefix_name";
	open(SUFFIX, "<$suffix_name") or die "Cannot open $suffix_name";

	$/ = undef;
	my $prefix = <PREFIX>; close(PREFIX);
	my $suffix = <SUFFIX>; close(SUFFIX);

	# Payload sizes.
	my $payload_size = length($suffix);
	my $payload_bss  = $bss_size;

	# Update the image size
	my $hdr_off          = unpack("V",substr($prefix, 0x3c, 4));
	my $image_size_off   = 0x050 + $hdr_off;
	my $img_mem_size_off = 0x0c0 + $hdr_off;
	my $img_size_off    =  0x0c8 + $hdr_off;

	my $image_size   = unpack("V", substr($prefix, $image_size_off, 4));
	my $img_mem_size = unpack("V", substr($prefix, $img_mem_size_off, 4));
	my $img_size     = unpack("V", substr($prefix, $img_size_off, 4));

	$image_size   += $payload_size + $payload_bss;
	$img_mem_size += $payload_size + $payload_bss;
	$img_size     += $payload_size;

	substr($prefix, $image_size_off, 4)   = pack("V", $image_size);
	substr($prefix, $img_mem_size_off, 4) = pack("V", $img_mem_size);
	substr($prefix, $img_size_off, 4)     = pack("V", $img_size);

	#print(STDERR "image_size:   $image_size\n");
	#print(STDERR "img_mem_size: $img_mem_size\n");
	#print(STDERR "img_size:     $img_size\n");

	print $prefix;
	print $suffix;
}

