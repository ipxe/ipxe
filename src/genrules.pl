#!/usr/bin/perl -w
#
#	Helper program to generate Makefile rules into file Rom from table in
#	file NIC
#
#	GPL, Ken Yap 2001, with major contributions by Klaus Espenlaub
#	Revised 2002
#

use strict;

use bytes;

use File::Basename;

use vars qw($familyfile $nic @families $curfam %drivers %pcient %isaent %isalist %buildent $arch @srcs);

sub __gendep ($$$)
{
	my ($file, $deps, $driver_dep) = @_;
	foreach my $source (@$deps) {
		my $inc;
		my @collect_dep = ();
		$inc = "arch/$arch/include/$source" unless ! -e "arch/$arch/include/$source";
		$inc = "include/$source" unless ! -e "include/$source";
		$inc = dirname($file) . "/$source" unless ! -e dirname($file) . "/$source";
		unless (defined($inc)) {
			print STDERR "$source from $file not found (shouldn't happen)\n";
			next;
		};
		next if (exists ${$driver_dep}{$inc});
		${$driver_dep}{$inc} = $inc;
# Warn about failure to open, then skip, rather than soldiering on with the read
		unless (open(INFILE, "$inc")) {
			print STDERR "$inc: $! (shouldn't happen)\n";
			next;
		};
		while (<INFILE>) {
			chomp($_);
# This code is not very smart: no C comments or CPP conditionals processing is
# done.  This may cause unexpected (or incorrect) additional dependencies.
# However, ignoring the CPP conditionals is in some sense correct: we need to
# figure out a superset of all the headers for the driver source.  
			next unless (s/^\s*#include\s*"([^"]*)".*$/$1/);
# Ignore system includes, like the ones in osdep.h
			next if ($_ =~ m:^/:);
# Ignore "special" includes, like .buildserial.h
		        next if /^\./;
			push(@collect_dep, $_);
		}
		close(INFILE);
		if (@collect_dep) {
			&__gendep($inc, \@collect_dep, $driver_dep);
		}
	}
}

sub gendep ($) {
	my ($driver) = @_;

	# Automatically generate the dependencies for the driver sources.
	my %driver_dep = ();
	__gendep( "", [ $driver ], \%driver_dep);
	return sort values %driver_dep
}

# Make sure that every rom name exists only once.
# make will warn if it finds duplicate rules, but it is better to stop
sub checkduplicate (\%$$) {
	my ($anyent, $curfam, $romname) = @_;
	foreach my $family (@families) {
		if (exists($$anyent{$family})) {
			my $aref = $$anyent{$family};
			foreach my $entry (@$aref) {
				if ($entry->[0] eq $romname) {
					print STDERR "\nROM name $romname defined twice. Please correct.\n";
					exit 1;
				}
			}
		}
	}
}

sub genroms($) {
	my ($driver) = @_;
	
	# Automatically discover the ROMS this driver can produce.
	unless (open(INFILE, "$driver")) {
		print STDERR "$driver: %! (shouldn't happen)\n";
		next;
	};
	while (<INFILE>) {
		chomp($_);
		if ($_ =~ m/^\s*PCI_ROM\(\s*0x([0-9A-Fa-f]*)\s*,\s*0x([0-9A-Fa-f]*)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\)/) {

			# We store a list of PCI IDs and comments for each PC target
			my ($vendor_id, $device_id, $rom, $comment) = (hex($1), hex($2), $3, $4);
			my $ids = sprintf("0x%04x,0x%04x", $vendor_id, $device_id);
			checkduplicate(%pcient, $curfam, $rom);
			push(@{$pcient{$curfam}}, [$rom, $ids, $comment]);
		}
		elsif($_ =~ m/^\s*ISA_ROM\(\s*"([^"]*)"\s*,\s*"([^"]*)"\)/) {
			my ($rom, $comment) = ($1, $2);
			# We store the base driver file for each ISA target
			$isalist{$rom} = $curfam;
			$buildent{$rom} = 1;
			checkduplicate(%isaent, $curfam, $rom);
			push(@{$isaent{$curfam}}, [$rom, $comment]);
		}
		elsif($_ =~ m/^\s*PCI_ROM/ or $_ =~ m/^\s*ISA_ROM/) {
			# Complain since we cannot parse this. Of course it would be nicer if we could parse...
			print STDERR "\nFound incomplete PCI_ROM or ISA_ROM macro in file $driver.\n";
			print STDERR "ROM macros spanning more than one line are not supported,\n";
			print STDERR "please adjust $driver accordingly.\n\n\n";
			exit 1;
		}
	}
}

sub addfam ($) {
	my ($family) = @_;

	push(@families, $family);
	# We store the list of dependencies in the hash for each family
	my @deps = &gendep("$family.c");
	$drivers{$family} = join(' ', @deps);
	$pcient{$family} = [];
	genroms("$family.c");
}

sub addrom ($) {
	my ($rom, $ids, $comment) = split(' ', $_[0], 3);

	# defaults if missing
	$ids = '-' unless ($ids);
	$comment = $rom unless ($comment);
	if ($ids ne '-') {
		# We store a list of PCI IDs and comments for each PCI target
		checkduplicate(%pcient, $curfam, $rom);
		push(@{$pcient{$curfam}}, [$rom, $ids, $comment]);
	} else {
		# We store the base driver file for each ISA target
		$isalist{$rom} = $curfam;
		$buildent{$rom} = 1;
		checkduplicate(%isaent, $curfam, $rom);
		push(@{$isaent{$curfam}}, [$rom, $comment]);
	}
}

# Return true if this driver is ISA only
sub isaonly ($) {
	my $aref = $pcient{$_[0]};

	return ($#$aref < 0);
}

$#ARGV >= 1 or die "Usage: $0 Families bin/NIC arch sources...\n";
$familyfile = shift(@ARGV);
$nic  = shift(@ARGV);
$arch = shift(@ARGV);
@srcs = @ARGV;
open FAM, "<$familyfile" or die "Could not open $familyfile: $!\n";

$curfam = '';
while ( <FAM> ) {
	chomp($_);
	next if (/^\s*(#.*)?$/);
	my ($keyword) = split(' ', $_ , 2);
	if ($keyword eq 'family') {
		my ($keyword, $driver) = split(' ', $_, 2);
		$curfam = '';
		if (! -e "$driver.c") {
			print STDERR "Driver file $driver.c not found, skipping...\n";
			next;
		}
		if ($driver =~ "^arch" && $driver !~ "^arch/$arch") {
# This warning just makes noise for most compilations.
#			print STDERR "Driver file $driver.c not for arch $arch, skipping...\n";
			next;
		}
		&addfam($curfam = $driver);
	} else {
		# skip until we have a valid family
		next if ($curfam eq '');
		&addrom($_);
	}
}
close FAM;

open(N,">$nic") or die "$nic: $!\n";
print N <<EOF;
# This is an automatically generated file, do not edit
# It does not affect anything in the build, it's only for rom-o-matic

EOF
foreach my $family (@families) {
	print N "family\t$family\n";
	if (exists($pcient{$family})) {
		my $aref = $pcient{$family};
		foreach my $entry (@$aref) {
			my $rom = $entry->[0];
			my $ids = $entry->[1];
			my $comment = $entry->[2];
			print N "$rom\t$ids\t$comment\n";
		}
	}
	if (exists($isaent{$family})) {
		my $aref = $isaent{$family};
		foreach my $entry (@$aref) {
			my $rom = $entry->[0];
			my $comment = $entry->[1];
			print N "$rom\t-\t$comment\n";
		}
	}
	print N "\n";
}
close(N);

# Generate the normal source dependencies
print "# Core object file dependencies\n";
foreach my $source (@srcs) {
	next if ($source !~ '[.][cS]$');
	my @deps = &gendep($source);
	my $obj = $source;
	$obj =~ s/^.*?([^\/]+)\.[cS]/bin\/$1.o/;
	foreach my $dep (@deps) {
		print "$obj: $dep\n";
	}
	print("\n");
}

# Generate the assignments to DOBJS and BINS
print "# Driver object files and ROM image files\n";
print "DOBJS\t:=\n";
print "PCIOBJS\t:=\n";

print "# Target formats\n";
print "EB_ISOS\t:=\n";
print "EB_LISOS\t:=\n";
print "EB_COMS\t:=\n";
print "EB_EXES\t:=\n";
print "EB_LILOS\t:=\n";
print "EB_ZLILOS\t:=\n";
print "EB_PXES\t:=\n";
print "EB_ZPXES\t:=\n";
print "EB_DSKS\t:=\n";
print "EB_ZDSKS\t:=\n";
print "EB_ELFS\t:=\n";
print "EB_ZELFS\t:=\n";
print "EB_LMELFS\t:=\n";
print "EB_ZLMELFS\t:=\n";
print "EB_ELFDS\t:=\n";
print "EB_ZELFDS\t:=\n";
print "EB_LMELFDS\t:=\n";
print "EB_ZLMELFDS\t:=\n";

foreach my $pci (sort keys %pcient) {
	my $img = basename($pci);

	print "DOBJS\t+= \$(BIN)/$img.o\n";
	print "PCIOBJS\t+= \$(BIN)/$img.o\n" unless isaonly($pci);

# Output targets
	print "EB_LILOS\t+= \$(BIN)/$img.lilo \nEB_ZLILOS\t+= \$(BIN)/$img.zlilo\n";
	print "EB_PXES\t+= \$(BIN)/$img.pxe   \nEB_ZPXES\t+= \$(BIN)/$img.zpxe\n";
	print "EB_DSKS\t+= \$(BIN)/$img.dsk   \nEB_ZDSKS\t+= \$(BIN)/$img.zdsk\n";
	print "EB_ELFS\t+= \$(BIN)/$img.elf   \nEB_ZELFS\t+= \$(BIN)/$img.zelf\n";
	print "EB_LMELFS\t+= \$(BIN)/$img.lmelf \nEB_ZLMELFS\t+= \$(BIN)/$img.zlmelf\n";
	print "EB_ELFDS\t+= \$(BIN)/$img.elfd   \nEB_ZELFDS\t+= \$(BIN)/$img.zelfd\n";
	print "EB_LMELFDS\t+= \$(BIN)/$img.lmelfd \nEB_ZLMELFDS\t+= \$(BIN)/$img.zlmelfd\n";
	print "EB_BIMAGES\t+= \$(BIN)/$img.bImage \nEB_BZIMAGES\t+= \$(BIN)/$img.bzImage\n";
	print "EB_ISOS\t+= \$(BIN)/$img.iso\n";
	print "EB_LISOS\t+= \$(BIN)/$img.liso\n";
	print "EB_COMS\t+= \$(BIN)/$img.com\n";
	print "EB_EXES\t+= \$(BIN)/$img.exe\n";
}

foreach my $img (sort keys %buildent) {

	print "DOBJS\t+= \$(BIN)/$img.o\n";

# Output targets
	print "EB_LILOS\t+= \$(BIN)/$img.lilo \nEB_ZLILOS\t+= \$(BIN)/$img.zlilo\n";
	print "EB_PXES\t+= \$(BIN)/$img.pxe   \nEB_ZPXES\t+= \$(BIN)/$img.zpxe\n";
	print "EB_DSKS\t+= \$(BIN)/$img.dsk   \nEB_ZDSKS\t+= \$(BIN)/$img.zdsk\n";
	print "EB_ELFS\t+= \$(BIN)/$img.elf   \nEB_ZELFS\t+= \$(BIN)/$img.zelf\n";
	print "EB_LMELFS\t+= \$(BIN)/$img.lmelf \nEB_ZLMELFS\t+= \$(BIN)/$img.zlmelf\n";
	print "EB_ELFDS\t+= \$(BIN)/$img.elfd   \nEB_ZELFDS\t+= \$(BIN)/$img.zelfd\n";
	print "EB_LMELFDS\t+= \$(BIN)/$img.lmelfd \nEB_ZLMELFDS\t+= \$(BIN)/$img.zlmelfd\n";
	print "EB_BIMAGES\t+= \$(BIN)/$img.bImage \nEB_BZIMAGE\t+= \$(BIN)/$img.bzImage\n";
	print "EB_ISOS\t+= \$(BIN)/$img.iso\n";
	print "EB_LISOS\t+= \$(BIN)/$img.liso\n";
	print "EB_COMS\t+= \$(BIN)/$img.com\n";
	print "EB_EXES\t+= \$(BIN)/$img.exe\n";
}

print "ROMS\t:=\n";
foreach my $family (sort keys %pcient) {
	my $aref = $pcient{$family};
	foreach my $entry (@$aref) {
		my $rom = $entry->[0];
		print "ROMS\t+= \$(BIN)/$rom.rom \$(BIN)/$rom.zrom\n";
	}
}
foreach my $isa (sort keys %isalist) {
	print "ROMS\t+= \$(BIN)/$isa.rom \$(BIN)/$isa.zrom\n";
}

# Generate the *.o rules
print "\n# Rules to build the driver object files\n";
foreach my $pci (sort keys %drivers) {
	# For ISA the rule for .o will generated later
	next if isaonly($pci);
	# PCI drivers are compiled only once for all ROMs
	(my $macro = basename($pci)) =~ tr/\-/_/;
	my $obj = basename($pci);
	my $deps = $drivers{$pci};
	print <<EOF;

\$(BIN)/$obj.o:	$pci.c \$(MAKEDEPS) $deps
	\$(CC) \$(CFLAGS) \$(\U$macro\EFLAGS) -o \$@ -c \$<

\$(BIN)/$obj--%.o:	\$(BIN)/%.o \$(BIN)/$obj.o \$(MAKEDEPS)
	\$(LD) -r \$(BIN)/$obj.o \$< -o \$@

EOF
}

# Do the ISA entries
foreach my $isa (sort keys %isalist) {
	(my $macro = $isa) =~ tr/\-/_/;
	my $base = $isalist{$isa};
	my $deps = $drivers{$base};
	print <<EOF;
\$(BIN)/$isa.o:	$base.c \$(MAKEDEPS) $deps
	\$(CC) \$(CFLAGS) \$(\U$macro\EFLAGS) -o \$@ -c \$<

\$(BIN)/$isa--%.o:	\$(BIN)/%.o \$(BIN)/$isa.o \$(MAKEDEPS)
	\$(LD) -r \$(BIN)/$isa.o \$< -o \$@ 
EOF
}

# Generate the Rom rules
print "# Rules to build the ROM files\n";
foreach my $family (sort keys %pcient) {
	next if isaonly($family);
	my $img = basename($family);
	print <<EOF;
ROMTYPE_$img = PCI
EOF
	my $aref = $pcient{$family};
	foreach my $entry (@$aref) {
		my ($rom, $ids, $comment) = @$entry;
		next if ($ids eq '-');
		print <<EOF;
ROMTYPE_$rom = PCI
MAKEROM_ID_$rom = -p $ids

EOF
		next if($rom eq $img);
		print <<EOF;
\$(BIN)/$rom\%rom: \$(BIN)/$img\%rom
	cat \$< > \$@
	\$(MAKEROM) \$(MAKEROM_FLAGS) \$(MAKEROM_\$(ROMCARD)) \$(MAKEROM_ID_\$(ROMCARD)) -i\$(IDENT) \$@

EOF
	}
}

# ISA ROMs are prepared from the matching code images
# Think this can probably be removed, but not sure
foreach my $isa (sort keys %isalist) {
	print <<EOF;
EOF
}

