#!/usr/bin/perl -w

sub status_system ($$) {
	my ($command, $message) = @_;

	$status = system($command);
	$status <<= 8;
	if ($status < 0) {
		print STDERR "$!\n";
	}
	if ($status != 0) {
		print STDERR "$message\n";
	}
}

sub extendinitrd ($$) {
	my ($initrd, $nblocks) = @_;

	if ($nblocks <= 1440) {
		print STDERR "nblocks must be >= 1440\n";
		return (1);
	}
	(undef, $type, undef, $fnlen, undef)  = split(' ', `file $initrd`, 5);
	print "$type $fnlen\n";
	if ($type ne 'Minix' || $fnlen != 30) {
		die "Can only handle Minix initrds with 30 char filenames\n";
		return (1);
	}
	status_system("dd if=/dev/zero of=newinitrd bs=1k count=$nblocks", "Cannot create new initrd\n");
	status_system("mkfs.minix -n 30 newinitrd $nblocks", "Cannot mkfs.minix new initrd\n");
	mkdir("initrd.from") || print STDERR "Cannot make temp mount point initrd.from\n";
	mkdir("initrd.to") || print STDERR "Cannot make temp mount point initrd.to\n";
	status_system("mount -o ro,loop $initrd initrd.from", "Cannot mount $initrd on initrd.from");
	status_system("mount -o loop newinitrd initrd.to", "Cannot mount newinitrd on initrd.to");
	status_system("cp -a initrd.from/* initrd.to/", "Cannot copy initrd to newinitrd");
	status_system("umount initrd.from", "Cannot umount initrd.from");
	status_system("umount initrd.to", "Cannot umount initrd.to");
	rmdir("initrd.from") || print STDERR "Cannot remove temp mount point initrd.from\n";
	rmdir("initrd.to") || print STDERR "Cannot remove temp mount point initrd.to\n";
	return (0);
}

1;
