#!/usr/bin/perl -w

use constant SYSSIZE_LOC => 500;	# bytes from beginning of boot block
use constant MINSIZE => 32768;

use strict;

use bytes;

$#ARGV >= 1 or die "Usage: $0 liloprefix file ...\n";
open(L, "$ARGV[0]") or die "$ARGV[0]: $!\n";
undef($/);
my $liloprefix = <L>;
close(L);
length($liloprefix) >= 512 or die "LILO prefix too short\n";
shift(@ARGV);
my $totalsize = 0;
for my $file (@ARGV) {
	next if (! -f $file or ! -r $file);
	$totalsize += -s $file;
}
my $pad = 0;
if ($totalsize < MINSIZE) {
	$pad = MINSIZE - $totalsize;
	$totalsize = MINSIZE;
}
print STDERR "LILO payload is $totalsize bytes\n";
$totalsize += 16;
$totalsize >>= 4;
substr($liloprefix, SYSSIZE_LOC, 2) = pack('v', $totalsize);
print $liloprefix;
for my $file (@ARGV) {
	next unless open(I, "$file");
	undef($/);
	my $data = <I>;
	print $data;
	close(I);
}
print "\x0" x $pad;
exit(0);
