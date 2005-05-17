#!/usr/bin/perl -w
#
# Quick Perl program to decode and display details about 
# tagged images created by mknbi, and then mount the contained
# DOS filesystem using a loop-back mount
#
# Martin Atkins, November 1998
# by hacking disnbi by
# Ken Yap, September 1998
#
#

sub getvendordata {
	my ($flags) = @_;

	my $vendordata = '';
	my $vendorlen = ($flags & 0xff) >> 4;
	if ($vendorlen > 0) {
		$vendorlen *= 4;
		$vendordata = unpack("A$vendorlen", substr($imageheader, $curoffset));
		$curoffset += $vendorlen;
	}
	return ($vendordata);
}

sub decodesegmentflags {
	my ($flags) = @_;

	$flags >>= 24;
	$flags &= 0x3;
	($flags == 0) and $type = "Absolute";
	($flags == 1) and $type = "Follows last segment";
	($flags == 2) and $type = "Below end of memory";
	($flags == 3) and $type = "Below last segment loaded";
	return ($type);
}

sub onesegment
{
	my ($segnum) = @_;
	my ($type, $vendordata);

	my ($flags, $loadaddr, $imagelen, $memlength) = unpack("V4", substr($imageheader, $curoffset));
	$curoffset += 16;
	print "Segment number $segnum\n";
	printf "Load address:\t\t%08x\n", $loadaddr;
	printf "Image length:\t\t%d\n", $imagelen;
	printf "Memory length:\t\t%d\n", $memlength;
	$type = &decodesegmentflags($flags);
	printf "Position:\t\t$type\n";
	printf "Vendor tag:\t\t%d\n", ($flags >> 8) & 0xff;
	if (($vendordata = &getvendordata($flags)) ne '') {
		print "Vendor data:\t\t", $vendordata, "\n";
	}
	print "\n";
	push (@seglengths, $imagelen);
	return (($flags >> 26) & 1);
}

@seglengths = ();
$#ARGV == 1 or die "Usage: mntnbi tagged-image-file dir\n";
$imagefile= $ARGV[0];
open(I, $ARGV[0]) or die "$imagefile: $!\n";
(defined($status = sysread(I, $imageheader, 512)) and $status == 512)
	or die "$imagefile: Cannot read header\n";
$headerrecord = substr($imageheader, 0, 16);
($magic, $flags, $bx, $ds, $ip, $cs) = unpack("a4Vv4", $headerrecord);
$magic eq "\x36\x13\x03\x1B" or die "$imagefile: Not a tagged image file\n";
$curoffset = 16;

# Now decode the header

printf "Header location:\t%04x:%04x\n", $ds, $bx;
printf "Start address:\t\t%04x:%04x\n", $cs, $ip;
printf "Flags:\n";
	print "Return to loader after execution (extension)\n" if (($flags >> 8) &  1);
if (($vendordata = &getvendordata($flags)) ne '') {
	print "Vendor data:\t\t", $vendordata, "\n";
}
print "\n";

# Now decode each segment record

$segnum = 1;
do {
	$lastrecord = &onesegment($segnum);
	++$segnum;
} while (!$lastrecord);

if ($#seglengths != 1) {
	die "This is not a DOS image $#seglengths\n";
}
$offset = 512 + $seglengths[0];
print "mounting filesystem at offset $offset in $ARGV[0] on $ARGV[1]\n";
$rc = system "mount $ARGV[0] $ARGV[1] -t msdos -o loop,offset=$offset";
print "Done\n" if ($rc == 0);
exit(0);
