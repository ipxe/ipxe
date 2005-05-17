#!/usr/bin/perl -w
#
# If called as wake.pl -f file it reads lines of the form
#
# aa:bb:cc:dd:ee;ff 12.34.56.78 or
# aa:bb:cc:dd:ee:ff foo.bar.com
# aa:bb:cc:dd:ee:ff
#
# which are MAC addresses and hostnames of NICs to send a wakeup packet.
# In the first two cases, unicast is used (and root permission may be
# required if the ARP cache needs to be injected with a mapping).
# In the third case, broadcast is used, and anybody can run the command.
# Comments in the file start with #.
#
# Or MAC addresses can be specified on the command line
#
# wake.pl aa.bb.cc.dd.ee.ff
#
# Or both can be used:
#
# wake.pl -f addresses.cfg 11:22:33:44:55:66
#
# This program may have to be run with superuser privilege because it
# may need to inject an ARP entry into the cache.
# Be careful, you could corrupt valid entries if those NICs are
# already active.
#
# Perl version by ken_yap@users.sourceforge.net after DOS/Windows C version posted by
# Steve_Marfisi@3com.com on the Netboot mailing list
# Released under GNU Public License, 2000-01-08
# Added switch -q for quiet mode, changed Getopt usage to work with older versions,
# added switch -u for unicast mode, now default is always broadcast mode,
# added switch -s for seach pattern so you could filter entries from file.
# Guenter.Knauf@dialup.soco.de, 2000-10-14
#
use Getopt::Std;
use Socket;

getopts('quf:s:');
if (defined($opt_f)) {
	unless (open(F, $opt_f)) {
		print STDERR "open: $opt_f: $!\n";
	} else {
		while (<F>) {
			next if /^\s*#/;	# skip comments
			($mac, $ip) = split;
			next if !defined($mac) or $mac eq '';
			next if defined($opt_s) and (!/$opt_s/);
			if (!defined($ip) or $ip eq '' or !$opt_u) {
				&send_broadcast_packet($mac);
			} else {
				&send_unicast_packet($mac, $ip);
			}
		}
		close(F);
	}
}
while (@ARGV) {
	send_broadcast_packet(shift(@ARGV));
}

sub send_broadcast_packet {
	($mac) = @_;

	if ($mac !~ /^[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}$/i)  {
		print STDERR "Malformed MAC address $mac\n";
		return;
	}
	# Remove colons
	$mac =~ tr/://d;
	# Magic packet is 6 bytes of FF followed by the MAC address 16 times
	$magic = ("\xff" x 6) . (pack('H12', $mac) x 16);
	# Create socket
	socket(S, PF_INET, SOCK_DGRAM, getprotobyname('udp'))
		or die "socket: $!\n";
	# Enable broadcast
	setsockopt(S, SOL_SOCKET, SO_BROADCAST, 1)
		or die "setsockopt: $!\n";
	# Send the wakeup packet
	if (!$opt_q) {
		print "Sending wakeup packet to MAC address $mac";
		print " ($ip)" if (defined($ip));
		print "\n";
	}
	defined(send(S, $magic, 0, sockaddr_in(0x2fff, INADDR_BROADCAST)))
		or print STDERR "send: $!\n";
	close(S);
}

sub send_unicast_packet {
	($mac, $ip) = @_;

	if (!defined($iaddr = inet_aton($ip))) {
		print STDERR "Cannot resolve $ip\n";
		return;
	}
	if ($mac !~ /^[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}:[\da-f]{2}$/i)  {
		print STDERR "Malformed MAC address $mac\n";
		return;
	}
	# Inject entry into ARP table, in case it's not there already
	system("arp -s $ip $mac") == 0
		or print STDERR "Warning: arp command failed, you need to be root\n";
	# Remove colons
	$mac =~ tr/://d;
	# Magic packet is 6 bytes of FF followed by the MAC address 16 times
	$magic = ("\xff" x 6) . (pack('H12', $mac) x 16);
	# Create socket
	socket(S, PF_INET, SOCK_DGRAM, getprotobyname('udp'))
		or die "socket: $!\n";
	# Send the wakeup packet
	print "Sending wakeup packet to $ip at MAC address $mac\n" if (!$opt_q);
	defined(send(S, $magic, 0, sockaddr_in(0x2fff, $iaddr)))
		or print STDERR "send: $!\n";
	close(S);
}
