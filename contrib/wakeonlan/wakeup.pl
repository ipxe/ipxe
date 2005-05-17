#!/usr/bin/perl
#!/usr/bin/perl -w
#
# If called as wakeup.pl -f file it reads lines of the form
#
# aa:bb:cc:dd:ee;ff 12.34.56.78 or
# aa:bb:cc:dd:ee:ff foo.bar.com
# aa:bb:cc:dd:ee:ff
#
# which are MAC addresses and hostnames of NICs to send a wakeup packet.
# Broadcast is used to send the magic packets, so anybody can run the command.
# Notice that many routers do NOT forward broadcasts automatically!!
# Comments in the file start with #.
#
# Or MAC addresses can be specified on the command line
#
# wakeup.pl aa.bb.cc.dd.ee.ff
#
# Or both can be used:
#
# wakeup.pl -f addresses.cfg 11:22:33:44:55:66
# 
# Use option -b to specify broadcast mask.
# Use option -d for screen output.
#
# Perl version by ken.yap@acm.org after DOS/Windows C version posted by
# Steve_Marfisi@3com.com on the Netboot mailing list
# Released under GNU Public License, 2000-01-08
# Modified for use with NetWare by gk@gknw.de, 2000-09-18
# With NetWare you have to use Socket.NLP from NetWare Perl #334 or higher!
# You could download Socket.NLP #334 from: http://www.gknw.de/mpform.html
#

use Getopt::Std;
use Socket;

getopts('b:df:p:q');

$brc = $opt_b || '255.255.255.255';
$port = $opt_p || 60000;
die "Malformed broadcast address: $brc!\n" if ($brc !~ /^(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$)/);

if (defined($opt_f)) {
	unless (open(F, $opt_f)) {
		print "open: $opt_f: $!\n";
	} else {
		print "Using file $opt_f...\n" if ($opt_d);
		while (<F>) {
			next if /^\s*#/;	# skip comments
			my ($mac, $ip) = split;
			next if !defined($mac) or $mac eq '';
			&send_broadcast_packet($mac,$ip);
		}
		close(F);
	}
}
while (@ARGV) {
	send_broadcast_packet(shift(@ARGV));
}

sub send_broadcast_packet {
	my ($mac,$ip) = @_;
	if ($mac =~ /-/) {
		($bc,$mac) = split(/-/,$mac);
	} else {
		$bc = $brc;
	}
	if ($mac !~ /^[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}$/i) {
		print "Malformed MAC address $mac\n";
		return;
	}
	my $nbc = inet_aton($bc);
	# Remove colons
	$mac =~ tr/://d;
	# Magic packet is 6 bytes of FF followed by the MAC address 16 times
	$magic = ("\xff" x 6) . (pack('H12', $mac) x 16);
	# Create socket
	socket(S, PF_INET, SOCK_DGRAM, getprotobyname('udp')) or die "socket: $!\n";
	# Enable broadcast
	setsockopt(S, SOL_SOCKET, SO_BROADCAST, 1) or die "setsockopt: $!\n";
	# Send the wakeup packet
	printf("$0: Sending wakeup packet to %04X:%08X-%s %s\n",$port,unpack('N',$nbc),uc($mac),$ip) if ($opt_d);
	defined(send(S, $magic, 0, sockaddr_in($port, $nbc)))
		or print "send: $!\n";
	close(S);
}
