#!/usr/bin/perl -w
#
#	Program to display key information about a boot ROM
#	including PCI and PnP structures
#
#	GPL, Ken Yap 2001
#

use bytes;

sub getid ($)
{
	my ($offset) = @_;

	return ''  if ($offset == 0 or $offset > $len);
	my ($string) = unpack('Z32', substr($data, $offset, 32));
	return ($string);
}

sub dispci
{
	my ($pcidata) = substr($data, $pci, 0x18);
	my ($dummy, $vendorid, $deviceid, $vpd, $pcilen, $pcirev,
		$class1, $class2, $class3, $imglen, $coderev, $codetype,
		$indicator) = unpack('a4v4C4v2C2', $pcidata);
	$imglen *= 512;
	my $vendorstr = sprintf('%#04x', $vendorid);
	my $devicestr = sprintf('%#04x', $deviceid);
	my $coderevstr = sprintf('%#04x', $coderev);
	print <<EOF;
PCI structure:

Vital product data: $vpd
Vendor ID: $vendorstr
Device ID: $devicestr
Device base type: $class1
Device sub type: $class2
Device interface type: $class3
Image length: $imglen
Code revision: $coderevstr
Code type: $codetype
Indicator: $indicator

EOF
}

sub dispnp
{
	my ($pnpdata) = substr($data, $pnp, 0x20);
	my ($dummy1, $pnprev, $pnplen, $nextpnp, $dummy2,
		$pnpcsum, $deviceid, $mfrid, $productid,
		$class1, $class2, $class3, $indicator,
		$bcv, $dv, $bev, $dummy, $sri) = unpack('a4C2vC2a4v2C4v5', $pnpdata);
	print <<EOF;
PnP structure:

EOF
	print 'Vendor: ', &getid($mfrid), "\n";
	print 'Device: ', &getid($productid), "\n";
	my $indicatorstr = sprintf('%#02x', $indicator);
	my $bcvstr = sprintf('%#04x', $bcv);
	my $dvstr = sprintf('%#04x', $dv);
	my $bevstr = sprintf('%#04x', $bev);
	my $sristr = sprintf('%#04x', $sri);
	my $checksum = unpack('%8C*', $pnpdata);
	print <<EOF;
Device base type: $class1
Device sub type: $class2
Device interface type: $class3
Device indicator: $indicatorstr
Boot connection vector: $bcvstr
Disconnect vector: $dvstr
Bootstrap entry vector: $bevstr
Static resource information vector: $sristr
Checksum: $checksum

EOF
}

sub pcipnp
{
	($pci, $pnp) = unpack('v2', substr($data, 0x18, 4));
	if ($pci >= $len or $pnp >= $len) {
		print "$file: Not a PCI PnP ROM image\n";
		return;
	}
	if (substr($data, $pci, 4) ne 'PCIR' or substr($data, $pnp, 4) ne '$PnP') {
		print "$file: No PCI and PNP structures, not a PCI PNP ROM image\n";
		return;
	}
	&dispci();
	&dispnp();
}

$file = $#ARGV >= 0 ? $ARGV[0] : '-';
open(F, "$file") or die "$file: $!\n";
binmode(F);
# Handle up to 64kB ROM images
$len = read(F, $data, 64*1024);
close(F);
defined($len) or die "$file: $!\n";
substr($data, 0, 2) eq "\x55\xAA" or die "$file: Not a boot ROM image\n";
my ($codelen) = unpack('C', substr($data, 2, 1));
$codelen *= 512;
if ($codelen < $len) {
	my $pad = $len - $codelen;
	print "Image is $codelen bytes and has $pad bytes of padding following\n";
	$data = substr($data, 0, $codelen);
} elsif ($codelen > $len) {
	print "Image should be $codelen bytes but is truncated to $len bytes\n";}
&pcipnp();
($csum) = unpack('%8C*', $data);
print "ROM checksum: $csum \n";
exit(0);
