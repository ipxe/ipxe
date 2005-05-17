#!/usr/bin/perl -w
#
# Quick hack to convert /etc/bootptab to format required by ISC DHCPD
# This only outputs the fixed hosts portion of the config file
# You still have to provide the global options and the subnet scoping
#
# Turn $useipaddr on if you prefer to use IP addresses in the config file
# I run DNS so I prefer domain names
$useipaddr = 0;
# This will be appended to get the FQDN unless the hostname is already FQDN
$domainname = "ken.com.au";
$tftpdir = "/tftpdir/";
open(B, "/etc/bootptab") or die "/etc/bootptab: $!\n";
while(<B>) {
	if (/^[^a-z]/) {
		$prevline = $_;
		next;
	}
	chomp($_);
	($hostname, @tags) = split(/:/, $_, 5);
	($fqdn = $hostname) .= ".$domainname" unless($hostname =~ /\./);
	($macaddr) = grep(/^ha=/, @tags);
	$macaddr =~ s/ha=//;
	$macaddr =~ s/(..)(..)(..)(..)(..)(..)/$1:$2:$3:$4:$5:$6/g;
	($ipaddr) = grep(/^ip=/, @tags);
	$ipaddr =~ s/ip=//;
	($bootfile) = grep(/^bf=/, @tags);
	$bootfile =~ s/bf=//;
	$bootfile = $tftpdir . $bootfile;
# I have a comment line above most entries and I like to carry this over
	print $prevline if ($prevline =~ /^#/);
	$address = $useipaddr ? $ipaddr : $fqdn;
	print <<EOF
	host $hostname {
		hardware ethernet $macaddr;
		fixed-address $address;
		filename "$bootfile";
	}
EOF
;
	$prevline = $_;
}
