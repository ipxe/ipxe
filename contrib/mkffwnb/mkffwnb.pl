#!/usr/bin/perl -w
#
# Perl script to make a bootable image from a floppyfw floppy
# The basic idea is to unpack and replace or convert all
# the necessary config files into the initrd
# and then make a bootable image out of it
#
# The --format= option overrides the default of nbi or elf hardcoded
# in the source. Valid arguments are nbi or elf.
#
# The --output= options specifies an output file instead of stdout
# The --nonet option specifies that a netbootable image is not to
# be built but the vmlinuz and initrd.gz files left behind in $tempdir
# The --localtime=f option specifies a timezone file that's to be
# copied to /etc/localtime in the initrd, allowing a different timezone.
# The --ffw29 option is intended for 2.9.x and above and extends
# the size of the initrd by making a bigger one and copying the original over.
#
# The first non-option argument is taken to be the letter of a floppy to
# convert, e.g. a:, b: or even x: where x: is mapped to a file using
# mtools mapping in $HOME/.mtoolsrc. See the mtools documentation.
# Thus you can work on a floppy image in a disk file and only write
# to a floppy with dd or cp when you need to test the image.

use Getopt::Long;

use lib '/usr/local/lib/mkffwnb/';
use Extendinitrd;

use strict;

use vars qw($testing $verbose $localtime $nonet $format $ffw29 $imagefile
	$floppy $libdir $tftpdir $output $tempdir $tempmount);

sub findversion () {
	my ($version) = grep(/FloppyFW/, `mtype $imagefile ${floppy}floppyfw.msg`);
	return '' unless defined($version) and $version ne '';
	chomp($version);
	$version =~ s/.*FloppyFW (\d+\.\d+\.\d+(\.\d+)?).*/$1/;
	return ($version);
}

sub getappendargs () {
	my ($append) = join(' ', grep(/^\s*(append\s|console=)/, `mtype $imagefile ${floppy}syslinux.cfg`));
	chomp ($append);
	my @args = split(/\s+/, $append);
	my @result = ();
	foreach $_ (@args) {
		next if (/^$/ or /^append/ or /^initrd=/);
		next if (!$ffw29 and /^root=/);
		push (@result, $_);
	}
	return (join(' ', @result));
}

# Copy whole floppy to the current directory
# m preserves timestamps, n overwrites without warning and / means recursive
sub mcopy ($) {
	my ($tempdir) = @_;

	print "mcopy $imagefile -mn/ ${floppy}* $tempdir\n";
	my $status = system("mcopy -mn/ $imagefile ${floppy}* $tempdir");
	return ($status / 256);
}

# Gunzip file, -f forces overwriting of uncompressed file
sub gunzip ($) {
	my ($file) = @_;

	print "Gunzipping $file\n" if ($verbose);
	my $status = system('gunzip', '-f', $file);
	return ($status / 256);
}

# Gzip file, -f forces overwriting of compressed file
sub gzip ($) {
	my ($file) = @_;

	print "Gzipping $file\n" if ($verbose);
	my $status = system('gzip', '-9', '-f', $file);
	return ($status / 256);
}

sub loopbackmount ($$) {
	my ($file, $point) = @_;

	print "Mounting $file on $point loopback\n" if ($verbose);
	my $status = system('mount', '-o', 'loop', $file, $point);
	return ($testing ? 0 : $status / 256);
}

sub loopbackumount ($) {
	my ($point) = @_;

	print "Umounting $point\n" if ($verbose);
	my $status = system('umount', $point);
	return ($testing ? 0 : $status / 256);
}

# Convert DOS CR-NL to Unix NL. $dst has implied prefix of $tempmount
# Use @output for temporary storage in case we write back to the same file
sub dostounix ($$) {
	my ($src, $dst) = @_;
	my @output = ();

	$dst = "$tempmount/$dst";
	print "Converting $src to $dst\n" if ($verbose);
	unless (open(S, $src)) {
		print "$src: $!\n";
		return (0);
	}
	while (<S>) {
		chomp;
		tr /\015//d;
		push(@output, $_);
	}
	close(S);
	open(D, ">$dst") or return;
	for $_ (@output) {
		print D "$_\n";
	}
	close(D);
	chmod(0755, $dst);
	return (1);
}

sub bunzip2untar ($$) {
	my ($file, $dir) = @_;

	print "Unpacking $file into $dir\n" if ($verbose);
	system("bunzip2 < $file | (cd $dir; tar xf -)");
}

$testing = $< != 0;
$verbose = 1;
$format = '';
$imagefile = '';
GetOptions('output=s' => \$output,
	'nonet!' => \$nonet,
	'localtime=s' => \$localtime,
	'format=s' => \$format,
	'ffw29!' => \$ffw29,
	'ffw30!' => \$ffw29,
	'i=s' => \$imagefile);
if (defined($output) and $output !~ m(^/)) {
	my $d = `pwd`;
	chomp($d);
	$output = "$d/$output";
}
if ($imagefile) {
	$imagefile = "-i $imagefile";
}
$libdir = '/usr/local/lib/mkffwnb';
$tftpdir = '/usr/local/var/tftpboot';
# default can also be 'elf'
$format = 'nbi' if ($format ne 'elf' and $format ne 'nbi');
$floppy = $#ARGV >= 0 ? $ARGV[0] : 'a:';
print <<EOF;
This program requires mtools, tar, bzip2, loopback mount in the kernel,
and root privileges to execute. Hope you have them.
EOF
my $version = &findversion();
$version ne '' or die "Cannot determine version\n";
print "Version $version\n";
my $append = &getappendargs();
$append = "--append='$append'" if $append ne '';
print "$append\n";
$libdir .= '/' . $version;
-d $libdir or die "Cannot find files for $version\n";
$tempdir = $nonet ? '/tmp/mkffwnb' : "/tmp/mkffwnb$$";
$tempmount = 'tmpmount';
mkdir($tempdir, 0755);
print "Copying files off floppy, please be patient...\n";
&mcopy($tempdir) == 0 or die "Mcopy failed, diskette problem?\n";
chdir($tempdir);
&gunzip('initrd.gz') == 0 or die "Gunzip of initrd.gz failed\n";
if ($ffw29) {
	extendinitrd("initrd", 5760);
	system("mv newinitrd initrd");
}
mkdir($tempmount, 0755);
&loopbackmount('initrd', $tempmount) == 0 or die "Loopback mount failed\n";
&dostounix("$libdir/linuxrc", "linuxrc") if (-r "$libdir/linuxrc");
unless (&dostounix("$libdir/floppyfw.ini", "floppyfw.ini")) {
	&dostounix("floppyfw/floppyfw.ini", $ffw29 ? "etc/floppyfw.ini" : "floppyfw.ini");
}
&dostounix("config", $ffw29 ? "etc/config.prelogin" : "etc/config");
for my $i (glob('*.bz2 floppyfw/add.bz2 modules/*.bz2 packages/*.bz2')) {
	&bunzip2untar($i, $tempmount);
}
for my $i (glob('packages/*.ini')) {
	my $file = $i;
	$file =~ s:packages/::;
	&dostounix($i, "etc/$file");
}
&dostounix("hosts", "etc/hosts");
&dostounix("modules.lst", "etc/modules.lst");
&dostounix("network.ini", "etc/network.init");
&dostounix("firewall.ini", "etc/firewall.init");
&dostounix("syslog.cfg", "etc/syslog.conf");
&dostounix("packages/timeinfo", "etc/localtime");
system("cp -p licenses/* $tempmount/licenses/");
# This conditional code is for 1.1.2 and below
unless (glob('modules/*.bz2')) {
	print "Copying additional modules\n" if ($verbose);
	system("cp -p modules/* $tempmount/lib/modules/");
}
# If a timezone file has been specified, copy that onto initrd
if (defined($localtime)) {
	if (-r $localtime) {
		print "Copying $localtime to $tempmount/etc/localtime\n";
		system("cp -p $localtime $tempmount/etc/localtime");
	} else {
		print "$localtime: $!\n";
	}
}
&loopbackumount($tempmount) == 0 or die "Loopback umount failed\n";
&gzip('initrd') == 0 or die "Gzip of initrd failed\n";
if ($nonet) {
	print "Floppyfw directory in $tempdir\n";
} else {
	print "Calling mk$format-linux to make the netbootable image\n" if ($verbose);
	$output = "$tftpdir/floppyfw-$version.nb" if (!defined($output));
	system("mk$format-linux $append --output=$output vmlinuz initrd.gz");
	system("rm -fr $tempdir");
}
