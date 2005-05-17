#!/usr/bin/perl -w
#
# tftp to http proxy
# Copyright 2003 Ken Yap
# Released under GPL2
#

require 5.8.0;		# needs constant and the pack Z format behaviour

use bytes;		# to forestall Unicode interpretation of strings
use strict;

use Getopt::Long;
use Socket;
use Sys::Hostname;
use Sys::Syslog;
use LWP;
use POSIX 'setsid';

use constant PROGNAME => 't2hproxy';
use constant VERSION => '0.1';

use constant ETH_DATA_LEN => 1500;
use constant {
	TFTP_RRQ => 1, TFTP_WRQ => 2, TFTP_DATA => 3, TFTP_ACK => 4,
	TFTP_ERROR => 5, TFTP_OACK => 6
};
use constant {
	E_UNDEF => 0, E_FNF => 1, E_ACC => 2, E_DISK => 3, E_ILLOP => 4,
	E_UTID => 5, E_FEXIST => 6, E_NOUSER => 7
};

use vars qw($prefix $proxy $sockh $timeout %options $tsize $bsize);

# We can't use die because xinetd will think something's wrong
sub log_and_exit ($) {
	syslog('info', $_[0]);
	exit;
}

sub what_source ($) {
	my ($port, $saddr) = sockaddr_in($_[0]);
	my $host = gethostbyaddr($saddr, AF_INET);
	return ($host, $port);
}

sub send_error ($$$) {
	my ($iaddr, $error, $message) = @_;
	# error packets don't get acked
	send(STDOUT, pack('nna*', TFTP_ERROR, $error, $message), 0, $iaddr);
}

sub send_ack_retry ($$$$$) {
	my ($iaddr, $udptimeout, $maxretries, $blockno, $sendfunc) = @_;
RETRY:
	while ($maxretries-- > 0) {
		&$sendfunc;
		my $rin = '';
		my $rout = '';
		vec($rin, fileno($sockh), 1) = 1;
		do {
			my ($fds, $timeleft) = select($rout = $rin, undef, undef, $udptimeout);
			last if ($fds <= 0);
			my $ack;
			my $theiripaddr = recv($sockh, $ack, 256, 0);
			# check it's for us
			if ($theiripaddr eq $iaddr) {
				my ($opcode, $ackblock) = unpack('nn', $ack);
				return (0) if ($opcode == TFTP_ERROR);
				# check that the right block was acked
				if ($ackblock == $blockno) {
					return (1);
				} else {
					syslog('info', "Resending block $blockno");
					next RETRY;
				}
			}
			# stray packet for some other server instance
			send_error($theiripaddr, E_UTID, 'Wrong TID');
		} while (1);
	}
	return (0);
}

sub handle_options ($$) {
	my ($iaddr, $operand) = @_;
	while ($operand ne '') {
		my ($key, $value) = unpack('Z*Z*', $operand);
		$options{$key} = $value;
		syslog('info', "$key=$value");
		$operand = substr($operand, length($key) + length($value) + 2);
	}
	my $optstr = '';
	if (exists($options{blksize})) {
		$bsize = $options{blksize};
		$bsize = 512 if ($bsize < 512);
		$bsize = 1432 if ($bsize > 1432);
		$optstr .= pack('Z*Z*', 'blksize', $bsize . '');
	}
	# OACK expects an ack for block 0
	log_and_exit('Abort received or retransmit limit reached, exiting')
		unless send_ack_retry($iaddr, 2, 5, 0,
		sub { send($sockh, pack('na*', TFTP_OACK, $optstr), 0, $iaddr); });
}

sub http_get ($) {
	my ($url) = @_;
	syslog('info', "GET $url");
	my $ua = LWP::UserAgent->new;
	$ua->timeout($timeout);
	$ua->proxy(['http', 'ftp'], $proxy) if (defined($proxy) and $proxy);
	my $req = HTTP::Request->new(GET => $url);
	my $res = $ua->request($req);
	return ($res->is_success, $res->status_line, $res->content_ref);
}

sub send_file ($$) {
	my ($iaddr, $contentref) = @_;
	my $blockno = 1;
	my $data;
	do {
		$blockno &= 0xffff;
		$data = substr($$contentref, ($blockno - 1) * $bsize, $bsize);
		# syslog('info', "Block $blockno length " . length($data));
		log_and_exit('Abort received or retransmit limit reached, exiting')
			unless send_ack_retry($iaddr, 2, 5, $blockno,
			sub { send($sockh, pack('nna*', TFTP_DATA, $blockno, $data), 0, $iaddr); });
		$blockno++;
	} while (length($data) >= $bsize);
}

sub do_rrq ($$) {
	my ($iaddr, $packetref) = @_;
	# fork and handle request in child so that *inetd can continue
	# to serve incoming requests
	defined(my $pid = fork) or log_and_exit("Can't fork: $!");
	exit if $pid;		# parent exits
	setsid or log_and_exit("Can't start a new session: $!");
	socket(SOCK, PF_INET, SOCK_DGRAM, getprotobyname('udp')) or log_and_exit('Cannot create UDP socket');
	$sockh = *SOCK{IO};
	my ($opcode, $operand) = unpack('na*', $$packetref);
	my ($filename, $mode) = unpack('Z*Z*', $operand);
	syslog('info', "RRQ $filename $mode");
	my $length = length($filename) + length($mode) + 2;
	$operand = substr($operand, $length);
	handle_options($iaddr, $operand) if ($operand ne '');
	my ($success, $status_line, $result) = http_get($prefix . $filename);
	syslog('info', $status_line);
	if ($success) {
		send_file($iaddr, $result);
	} else {
		send_error($iaddr, E_FNF, $status_line);
	}
}

$prefix = 'http://localhost/';
$timeout = 60;
GetOptions('prefix=s' => \$prefix,
	'proxy=s' => \$proxy,
	'timeout=i' => \$timeout);
$bsize = 512;
openlog(PROGNAME, 'cons,pid', 'user');
syslog('info', PROGNAME . ' version ' . VERSION);
my $packet;
my $theiriaddr = recv(STDIN, $packet, ETH_DATA_LEN, 0);
my ($host, $port) = what_source($theiriaddr);
syslog('info', "Connection from $host:$port");
my $opcode = unpack('n', $packet);
if ($opcode == TFTP_RRQ) {
	do_rrq($theiriaddr, \$packet);
} else {	# anything else is an error
	send_error($theiriaddr, E_ILLOP, 'Illegal operation');
}
exit 0;
