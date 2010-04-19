#!/usr/bin/perl -w

use File::Spec::Functions qw ( :ALL );
use File::Find;
use File::Path;
use FindBin;
use strict;
use warnings;

sub try_import_file {
  my $ipxedir = shift;
  my $edkdirs = shift;
  my $filename = shift;

  # Skip everything except headers
  return unless $filename =~ /\.h$/;
  print "$filename...";

  my $outfile = catfile ( $ipxedir, $filename );
  foreach my $edkdir ( @$edkdirs ) {
    my $infile = catfile ( $edkdir, $filename );
    if ( -e $infile ) {
      # We have found a matching source file - import it
      print "$infile\n";
      open my $infh, "<$infile" or die "Could not open $infile: $!\n";
      ( undef, my $outdir, undef ) = splitpath ( $outfile );
      mkpath ( $outdir );
      open my $outfh, ">$outfile" or die "Could not open $outfile: $!\n";
      my @dependencies = ();
      while ( <$infh> ) {
	# Strip CR and trailing whitespace
	s/\r//g;
	s/\s*$//g;
	chomp;
	# Update include lines, and record included files
	if ( s/^\#include\s+[<\"](\S+)[>\"]/\#include <ipxe\/efi\/$1>/ ) {
	  push @dependencies, $1;
	}
	print $outfh "$_\n";
      }
      close $outfh;
      close $infh;
      # Recurse to handle any included files that we don't already have
      foreach my $dependency ( @dependencies ) {
	if ( ! -e catfile ( $ipxedir, $dependency ) ) {
	  print "...following dependency on $dependency\n";
	  try_import_file ( $ipxedir, $edkdirs, $dependency );
	}
      }
      return;
    }
  }
  print "no equivalent found\n";
}

# Identify edk import directories
die "Syntax $0 /path/to/edk2/edk2\n" unless @ARGV == 1;
my $edktop = shift;
die "Directory \"$edktop\" does not appear to contain the EFI EDK2\n"
    unless -e catfile ( $edktop, "MdePkg" );
my $edkdirs = [ catfile ( $edktop, "MdePkg/Include" ),
		catfile ( $edktop, "IntelFrameworkPkg/Include" ) ];

# Identify iPXE EFI includes directory
my $ipxedir = $FindBin::Bin;
die "Directory \"$ipxedir\" does not appear to contain the iPXE EFI includes\n"
    unless -e catfile ( $ipxedir, "../../../include/ipxe/efi" );

print "Importing EFI headers into $ipxedir\nfrom ";
print join ( "\n and ", @$edkdirs )."\n";

# Import headers
find ( { wanted => sub {
  try_import_file ( $ipxedir, $edkdirs, abs2rel ( $_, $ipxedir ) );
}, no_chdir => 1 }, $ipxedir );
