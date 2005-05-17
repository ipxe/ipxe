#!/usr/bin/perl -w
#
# A program to make a netbootable image from a LRP firewall floppy
#
# Tested on a Dachstein Linux floppy image available from
# http://lrp1.steinkuehler.net/ or via http://leaf.sourceforge.net/

# The most recent version of this script and a companion HowTo is available at
# http://members.optushome.com.au/graybeard/linux/netboot.html
#
# Modified from the mklrpnb file found in the contrib/mklrpnb directory of the
# Etherboot source at http://etherboot.sourceforge.net/
#
# Modifications by Glenn McK <graybeard@users.sourceforge.net> 
# $Id$
##################################### 

# this entry will need changing
$image = "/home/graybeard/etherboot/dachstein-v1.0.2-1680.bin";

# these can remain, but change them if desired
#
# the next argument defaults to firewall if no other name is passed via the
# command line, this will be the directory where distribution will be expanded
# under $base and also the directory in /tftpboot for lrp.nb

my $uniqdir = shift || 'firewall';

$mntdir   = "/mnt/floppy";          # where the above image file can be mounted
$tftpbase = "/tftpboot";
$tftpboot = "$tftpbase/$uniqdir";   # where the netboot images will be available
$base     = "/usr/src/LRP";
$dachorg = "$base/dach-org-$uniqdir"; # a copy required to make the distribution
$dachnew = "$base/lrp-$uniqdir";      # the base files for the new distribution
$packages = "$dachnew/var/lib/lrpkg"; # list to allow lrcfg to display Packages

# everything below should be okay
######################################

if ( !-e $image ) {
    print
"\n\tA valid LRP file and directory are required\n\tdownload one then edit $0\n\n";
    exit 1;
}
if ( !-d $base ) {
    mkdir( $base, 0700 );
}

if ( !-d $dachorg ) {
    mkdir( $dachorg, 0700 );
}

if ( !-d $dachnew ) {
    mkdir( $dachnew, 0700 );
    `umount $mntdir`;
    `mount -o ro,loop $image $mntdir`;

    `cp -vr $mntdir/* $dachorg/`;

    @cfg = `cat $mntdir/syslinux.cfg`;

    unless ( defined(@cfg) ) {
        print "Cannot find syslinux.cfg on $mntdir\n";
        exit 1;
    }
    print "cfg = @cfg\n";
    ($append) = grep( /append/, @cfg );    # find the append= line
    print "append = \n$append\n";
    chomp($append);                        # remove trailing newline
    $append =~ s/append=//;                # remove the append= at beginning
    print "strip append = \n$append\n\n";
    @args = split ( / /, $append );        # split into arguments at whitespace
    ($root) = grep( /^initrd=/, @args );   # find the initrd= argument
    $root =~ s/^initrd=//;                 # remove the initrd= at beginning
    $root =~ s/\.lrp$//;                   # cleanup for paclages list
    print "strip initrd = \n$root\n\n";
    ($lrp) = grep( /^LRP=/, @args );       # find the LRP= argument
    $lrp =~ s/^LRP=//;                     # remove the LRP= at beginning
    print "strip LRP =\n$lrp\n\n";
    @lrp = split ( /,/, $lrp );            # split into filenames at ,
    unshift ( @lrp, $root );               # prepend the root LRP filename
    @pack = @lrp;
    print "LRP =\n@lrp\n\n";
    $append = '';

    foreach $i (@args) {                   # rebuild the append string
        next if ( $i =~ /^initrd=/ );      # minus the unneeded parameters
        next if ( $i =~ /^LRP=/ );
        next if ( $i =~ /^boot=/ );
        next if ( $i =~ /^PKGPATH=/ );
        print "$i = i\n";
        $append .= "$i ";
    }

    print "final append = \n$append\n";

    chdir($dachnew) or die "$dachnew: $!\n";
    foreach $i (@lrp) {
        $i .= '.lrp' if $i !~ /\.lrp$/;
        print "\n\n\nUnpacking $i\n";
        system("ln -svf $dachorg/$i ${dachorg}/${i}.tar.gz");
        chmod 0600, "$dachorg/$i";
        system("cat $mntdir/$i | tar zxvf -");
    }

    # create file for lrcfg to display packages
    open( PACKAGES, ">$packages/packages" )
      || print "unable to modify $packages:$!\n";
    foreach $line (@pack) {
        print PACKAGES "$line\n";
    }
    close PACKAGES;

    # prevent previous file from being overwritten during installation
    # and also mess with some values in /linuxrc to hide non errors
    open( LINUXRC, "$packages/root.linuxrc" );
    @text = <LINUXRC>;
    close LINUXRC;
    open( LINUXRC, ">$packages/root.linuxrc" );
    foreach $line (@text) {
        $line =~ s/PFX\/packages/PFX\/packages-old \
\t\t\t\t# packages changed to packages-old for netboot setup/;
        $line =~
s/^rc=1/# rc=1 changed to rc=0 to suppress error messages for netboot setup \
rc=0/;
        $line =~
s/echo -n \" \(nf\!\)\"/#echo -n \" \(nf\!\)\" changed to reflect ToDo list \
\t\t\techo -n \" netboot setup - No backups possible from this machine - ToFix ?"/;
        print LINUXRC $line;
    }
    close LINUXRC;

    # swap interfaces around in network config file
    # eth1 is the new external eth0 is OUR internal server access
    open( NETWORK, "$dachnew/etc/network.conf" )
      || print "Unable to modify NETWORK:$!\n";
    @text = <NETWORK>;
    close NETWORK;
    open( NETWORK, ">$dachnew/etc/network.conf" )
      || print "Unable to modify NETWORK:$!\n";
    foreach $line (@text) {
        $line =~ s/eth0/eth00/;
        $line =~ s/eth1/eth0/;
        $line =~ s/eth00/eth1/;
        print NETWORK $line;
    }
    close NETWORK;

    `echo $append > $dachorg/appendstr`;

    `umount /mnt/floppy`;
    print "\nThe files have been extracted to $dachnew\n";
    system("ls -al $dachnew");
}
else {
    print "\n\n\t$image \n \thas already been extracted to $dachnew \
\tNow skipping to the next step where the netboot file\
\twill be created.\n";

    $append = `cat $dachorg/appendstr`;
    print "\nThe new append string will be...\n$append\n";

    chdir($dachnew);
    if ( !-d $tftpbase ) {
        mkdir( $tftpbase, 0710 );
        system("chgrp nobody $tftpbase");
    }

    unlink($tftpboot);

    # these permissions really need changing to something secure
    mkdir( $tftpboot, 0710 );
    system("chgrp nobody $tftpboot");
    print "\tRepacking to $tftpboot/lrp.lrp\n";
    system("tar zcf $tftpboot/lrp.lrp *");
    print "\tExtracting kernel image from $dachorg\n";
    system("cat $dachorg/linux > $tftpboot/lrp.ker");
    print "\tCreating netboot image $tftpboot/lrp.nb\n";
    system(
"mknbi-linux --append='$append' --output=$tftpboot/lrp.nb $tftpboot/lrp.ker $tftpboot/lrp.lrp"
    );
    chmod 0604, "$tftpboot/lrp.nb", "$tftpboot/lrp.ker", "$tftpboot/lrp.lrp";
    print "\nThese netboot files are in $tftpboot\n";
    system("ls -al $tftpboot");
    print "\n   The owner and permissions for $tftpboot \
 and files should be checked for security. The above\
permissions assume that tftp is running chroot (nobody)
      drwx--r---   root:nobody   /tftpboot\n\n";
}

exit 0;
