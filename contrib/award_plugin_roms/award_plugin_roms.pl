#!/usr/bin/perl -w
use strict;
use FileHandle;
use integer;

sub unsigned_little_endian_to_value
{
	# Assumes the data is initially little endian
	my ($buffer) = @_;
	my $bytes = length($buffer);
	my $value = 0;
	my $i;
	for($i = $bytes -1; $i >= 0; $i--) {
		my $byte = unpack('C', substr($buffer, $i, 1));
		$value = ($value * 256) + $byte;
	}
	return $value;
}

sub decode_fixed_string
{
	my ($data, $bytes) = @_;
	return $data;
}

sub decode_pstring
{
	my ($buf_ref, $offset_ref) = @_;
	# Decode a pascal string
	my $offset = ${$offset_ref};
	my $len = unpack('C',substr(${$buf_ref}, $offset, 1));
	my $data = substr(${$buf_ref}, $offset +1,  $len);
	${$offset_ref} = $offset + $len +1;
	return $data;
}

sub decode_cstring
{
	# Decode a c string
	my ($buf_ref, $offset_ref) = @_;
	my ($data, $byte);
	my $index = ${$offset_ref};
	while(1) {
		$byte = substr(${$buf_ref}, $index, 1);
		if (!defined($byte) || ($byte eq "\0")) {
			last;
		}
		$data .= $byte;
		$index++;
	}
	${$offset_ref} = $index;
	return $data;
}

sub type_size
{
	my ($entry) = @_;
	my %type_length = (
		byte => 1,
		half => 2,
		word => 4,
		xword => 8,
		'fixed-string' => $entry->[2],
		pstring => 0,
		cstring => 0,
	);
	my $type = $entry->[0];
	if (!exists($type_length{$type})) {
		 die "unknown type $type";
	 }
	my $length = $type_length{$type};
	return $length;
}

sub decode_fixed_type
{
	my ($type, $data, $bytes) = @_;
	my %decoders = (
		'byte' => \&unsigned_little_endian_to_value,
		'half' => \&unsigned_little_endian_to_value,
		'word' => \&unsigned_little_endian_to_value,
		'xword' => \&unsigned_little_endian_to_value,
		'fixed-string' => \&decode_fixed_string,
	);
	my $decoder = $decoders{$type} or die "unknow fixed type $type";
	return $decoder->($data, $bytes);
}

sub decode_variable_type
{
	my ($type, $buf_ref, $offset_ref) = @_;
	my %decoders = (
		'pstring' => \&decode_pstring,
		'cstring' => \&decode_cstring,
	);
	my $decoder = $decoders{$type} or die "unknow variable type $type";
	return $decoder->($buf_ref, $offset_ref);
}

sub decode_struct
{
	my ($buf_ref, $offset, $layout) = @_;
	my $initial_offset = $offset;
	my ($entry, %results);
	foreach $entry (@$layout) {
		my ($type, $name) = @$entry;
		my $bytes = type_size($entry);
		if ($bytes > 0) {
			my $data = substr(${$buf_ref}, $offset, $bytes);
			$results{$name} = decode_fixed_type($type, $data, $bytes);
			$offset += $bytes;
		} else {
			$results{$name} = decode_variable_type($type, $buf_ref, \$offset);
		}
	}
	return (\%results, $offset - $initial_offset);
}

sub print_big_hex
{
	my ($min_digits, $value) = @_;
	my @digits;
	while($min_digits > 0 || ($value > 0)) {
		my $digit = $value%16;
		$value /= 16;
		unshift(@digits, $digit);
		$min_digits--;
	}
	my $digit;
	foreach $digit (@digits) {
		printf("%01x", $digit);
	}
}



my %lha_signatures = (
	'-com-' => 1,	
	'-lhd-'	=> 1,
	'-lh0-'	=> 1,
	'-lh1-'	=> 1,
	'-lh2-'	=> 1,
	'-lh3-'	=> 1,
	'-lh4-'	=> 1,
	'-lh5-'	=> 1,
	'-lzs-' => 1,
	'-lz4-' => 1,
	'-lz5-' => 1,
	'-afx-'	=> 1,
	'-lzf-'	=> 1,
);

my %lha_os = (
	'M' => 'MS-DOS',
	'2' => 'OS/2',
	'9' => 'OS9',
	'K' => 'OS/68K',
	'3' => 'OS/386',
	'H' => 'HUMAN',
	'U' => 'UNIX',
	'C' => 'CP/M',
	'F' => 'FLEX',
	'm' => 'Mac',
	'R' => 'Runser',
	'T' => 'TownOS',
	'X' => 'XOSK',
	'A' => 'Amiga',
	'a' => 'atari',
	' ' => 'Award ROM',
);


my @lha_level_1_header = (
	[ 'byte',         'header_size' ],    # 1
	[ 'byte',         'header_sum', ],    # 2
	[ 'fixed-string', 'method_id', 5 ],   # 7
	[ 'word',         'skip_size', ],     # 11
	[ 'word',         'original_size' ],  # 15
	[ 'half',         'dos_time' ],       # 17
	[ 'half',         'dos_date' ],       # 19
    	[ 'byte',         'fixed'   ],        # 20
	[ 'byte',         'level'   ],        # 21
	[ 'pstring',      'filename' ],       # 22
	[ 'half',         'crc' ],
	[ 'fixed-string', 'os_id', 1 ],
	[ 'half',         'ext_size' ],		  
);

# General lha_header
my @lha_header = (
	[ 'byte',         'header_size' ],
	[ 'byte',         'header_sum', ],
	[ 'fixed-string', 'method_id', 5 ],
	[ 'word',         'skip_size', ],
	[ 'word',         'original_size' ],
	[ 'half',         'dos_time' ],
	[ 'half',         'dos_date' ],
	[ 'half',         'rom_addr' ],
	[ 'half',         'rom_flags' ],
    	[ 'byte',         'fixed'   ],
	[ 'byte',         'level'   ],
	[ 'pstring',      'filename' ],
	[ 'half',         'crc' ],
	[ 'lha_os',	  'os_id', 1 ],
	[ 'half',         'ext_size' ],
	[ 'byte',         'zero' ],
	[ 'byte',         'total_checksum' ],
	[ 'half',         'total_size' ],
);

sub print_struct
{
	my ($layout, $self) = @_;
	my $entry;
	my $width = 0;
	foreach $entry(@$layout) {
		my ($type, $name) = @$entry;
		if (length($name) > $width) {
			$width = length($name);
		}
	}
	foreach $entry (@$layout) {
		my ($type, $name) = @$entry;
		printf("%*s = ", $width, $name);
		my $value = $self->{$name};
		if (!defined($value)) {
			print "undefined";
		}
		elsif ($type eq "lha_os") {
			print "$lha_os{$value}";
		}
		elsif ($type =~ m/string/) {
			print "$value";
		} 
		else {
			my $len = type_size($entry);
			print "0x";
			print_big_hex($len *2, $value);
		}
		print "\n";
	}
}

sub checksum
{
	my ($buf_ref, $offset, $length) = @_;
	my ($i, $sum);
	$sum = 0;
	for($i = 0; $i < $length; $i++) {
		my $byte = unpack('C', substr($$buf_ref, $offset + $i, 1));
		$sum = ($sum + $byte) %256;
	}
	return $sum;
}

sub decode_lha_header
{
	my ($buf_ref, $offset) = @_;
	my $level = unpack('C',substr(${$buf_ref}, $offset + 20, 1));

	my %self;
	my ($struct, $bytes);
	if ($level == 1) {
		($struct, $bytes) 
			= decode_struct($buf_ref, $offset, \@lha_level_1_header);
		%self = %$struct;
		if ($self{fixed} != 0x20) {
			 die "bad fixed value";
		}
		$self{total_size} = $self{header_size} + 2 + $self{skip_size};
		if ($bytes != $self{header_size} +2) {
			die "$bytes != $self{header_size} +2";
		}
		my $checksum = checksum($buf_ref, $offset +2, $self{header_size});
		if ($checksum != $self{header_sum}) {
			printf("WARN: Header bytes checksum to %02lx\n", 
				     $checksum);
		}
		# If we are an award rom...
		if ($self{os_id} eq ' ') {
			@self{qw(zero total_checksum)} = 
			    unpack('CC', substr($$buf_ref, 
				$offset + $self{total_size}, 2));
			if ($self{zero} != 0) {
				warn "Award ROM without trailing zero";
			}
			else {
				$self{total_size}++;
			}
			my $checksum = 
				checksum($buf_ref, $offset, $self{total_size});
			if ($self{total_checksum} != $checksum) {
				printf("WARN: Image bytes checksum to %02lx\n", 
					$checksum);
			}
			else {
				$self{total_size}++;
			}
			$self{rom_addr} = $self{dos_time};
			$self{rom_flags} = $self{dos_date};
			delete @self{qw(dos_time dos_date)};
		}
	}
	else {
		die "Unknown header type";
	}
	return \%self;
}

sub main
{
	my ($filename, $rom_length) = @_;
	my $fd = new FileHandle;
	if (!defined($rom_length)) {
		my ($dev, $ino, $mode, $nlink, $uid, $gid,$rdev,$size,
			$atime, $mtime, $ctime, $blksize, $blocks)
			= stat($filename);
		$rom_length = $size;
	}
	$fd->open("<$filename") or die "Cannot ope $filename";
	my $data;
	$fd->read($data, $rom_length);
	$fd->close();
	
	my $i;
	for($i = 0; $i < $rom_length; $i++) {
		my $sig = substr($data, $i, 5);
		if (exists($lha_signatures{$sig})) {
			my $start = $i -2;
			my $header = decode_lha_header(\$data, $start);
			
			my $length = $header->{total_size};
			print "AT:  $start - @{[$start + $length -1]},  $length bytes\n";
			print_struct(\@lha_header, $header);
			print "\n";

		}
	}
}

main(@ARGV);
