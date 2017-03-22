#!/usr/bin/env perl
#
# $FreeBSD$
#

use strict;
use warnings;
use locale;

use Getopt::Std;
use File::Basename;

my $prog = basename($0);

our $opt_h;
our $builddate;
our $svnrev;
our $junk = "";
our $arch = "";
our $kernel = "";
our $branch = "";
our $revision = 0;
our $branchname = "";
our $version = 0;
our $hasarmv6 = 0;
our $hasarm64 = 0;
our $hasbranch = 0;

sub usage() {
	print("Usage: ./get-checksums.sh -c ./builds-NN.conf | $prog > outfile\n");
	exit(0);
}

sub main() {
	getopts('h');
	my @lines = ();
	my @builds = ();
	my @amis = ();
	my @vmimages = ();
	my $endisos = 0;
	$builddate = 0;
	$svnrev = 0;
	$junk = "";
	$arch = "";
	$kernel = "";
	$branch = "";
	$branchname = "";
	$revision = 0;
	$version = 0;
	$hasbranch = 0;

	if ($opt_h) {
		&usage();
	}

	while(<STDIN>) {
		chomp($_);
		push(@lines, $_);
		if ($_ =~ m/^== VM IMAGE CHECKSUMS ==/) {
			$endisos = 1;
		}
		if ($_ =~ m/^BUILDDATE/) {
			$builddate = $_;
			$builddate =~ s/^BUILDDATE=//;
			pop(@lines);
			next;
		}
		if ($_ =~ m/^SVNREV/) {
			$svnrev = $_;
			$svnrev =~ s/^SVNREV=//;
			pop(@lines);
			next;
		}
		if ($_ =~ m/^o /) {
			$_ =~ s/:$//;
			if ($_ =~ m/^o .* armv6 .*/) {
				$hasarmv6 = 1;
			}
			if ($_ =~ m/^o .* aarch64 .*/) {
				$hasarm64 = 1;
			}
			if ($hasbranch eq 0) {
				($junk, $branch, $arch, $kernel) = split(" ", $_);
				$revision = $branch;
				$revision =~ s/-\w+.*//;
				$version = $revision;
				$version =~ s/\.\d.*//;
				$branchname = $branch;
				$branchname =~ s/\d+\.\d-//;
				$branchname =~ s/ .*$//;
				if ($branchname =~ m/(ALPHA|CURRENT)/) {
					$branch = "head";
				} elsif ($branchname =~ m/(BETA|PRERELEASE|RC|STABLE)/) {
					$branch = "stable/$version";
				} else {
					$branch = "unknown";
				}
				$hasbranch = 1;
			}
			if ($endisos == 0) {
				push(@builds, $_);
			} else {
				push(@vmimages, $_);
			}
		}
		if ($_ =~ m/^Created AMI in /) {
			$_ =~ s/^Created AMI in //;
			# Exclude ca-central-1 eu-west-2 for now
			#if ($_ !~ m/(ca-central-1|eu-west-2)/) {
				push(@amis, $_);
				pop(@lines);
			#}
		}
	}

	print <<HEADER;
To: freebsd-snapshots\@FreeBSD.org
Subject: New FreeBSD snapshots available: $branch ($builddate r$svnrev)

HEADER
	print <<OPENING;
New FreeBSD development branch installation ISOs and virtual machine
disk images have been uploaded to the FTP mirrors.

As with any development branch, the installation snapshots are not
intended for use on production systems.  We do, however, encourage
testing on non-production systems as much as possible.

Please also consider installing the sysutils/panicmail port, which can
help in providing FreeBSD developers the necessary information regarding
system crashes.

Checksums for the installation ISOs and the VM disk images follow at
the end of this email.

=== Installation ISOs ===

Installation images are available for:

OPENING

	foreach my $build (@builds) {
		print("$build\n");
	}

	if ($hasarmv6 ne 0) {
		print <<ARMINFO;

Note regarding arm/armv6 images: For convenience for those without
console access to the system, a freebsd user with a password of
freebsd is available by default for ssh(1) access.  Additionally,
the root user password is set to root, which it is strongly
recommended to change the password for both users after gaining
access to the system.
ARMINFO
	}

	print <<OPENING;

Snapshots may be downloaded from the corresponding architecture
directory from:

    ftp://ftp.freebsd.org/pub/FreeBSD/snapshots/ISO-IMAGES/

Please be patient if your local FTP mirror has not yet caught
up with the changes.

Problems, bug reports, or regression reports should be reported through
the Bugzilla PR system or the appropriate mailing list such as -current\@
or -stable\@ .

=== Virtual Machine Disk Images ===
 
VM disk images are available for the following architectures:

OPENING

	foreach my $vmimage (@vmimages) {
		print("$vmimage\n");
	}

	print <<OPENING;

Disk images may be downloaded from the following URL (or any of the
FreeBSD FTP mirrors):

    ftp://ftp.freebsd.org/pub/FreeBSD/snapshots/VM-IMAGES/

Images are available in the following disk image formats:

    ~ RAW
    ~ QCOW2 (qemu)
    ~ VMDK (qemu, VirtualBox, VMWare)
    ~ VHD (qemu, xen)

The partition layout is:

    ~ 512k - freebsd-boot GPT partition type (bootfs GPT label)
    ~ 1GB  - freebsd-swap GPT partition type (swapfs GPT label)
    ~ ~17GB - freebsd-ufs GPT partition type (rootfs GPT label)
OPENING

	if ($hasarm64 ne 0) {
		print <<AARCH64;

Note regarding arm64/aarch64 virtual machine images: a modified QEMU EFI
loader file is needed for qemu-system-aarch64 to be able to boot the
virtual machine images.  See this page for more information:

    https://wiki.freebsd.org/arm64/QEMU

To boot the VM image, run:

    % qemu-system-aarch64 -m 4096M -cpu cortex-a57 -M virt  \\
	-bios QEMU_EFI.fd -serial telnet::4444,server -nographic \\
	-drive if=none,file=VMDISK,id=hd0 \\
	-device virtio-blk-device,drive=hd0 \\
	-device virtio-net-device,netdev=net0 \\
	-netdev user,id=net0

Be sure to replace "VMDISK" with the path to the virtual machine image.
AARCH64
	}

	if ($#amis ne 0) {
	print <<AMIS;

=== Amazon EC2 AMI Images ===

FreeBSD/amd64 EC2 AMIs are available in the following regions:

AMIS
	}
	foreach my $ami (@amis) {
		print(" $ami\n");
	}

	print <<VAGRANT;

=== Vagrant Images ===

FreeBSD/amd64 images are available on the Hashicorp Atlas site for the
VMWare Desktop and VirtualBox providers, and can be installed by
running:

    % vagrant init freebsd/FreeBSD-$revision-$branchname
    % vagrant up

VAGRANT
	foreach my $line (@lines) {
		if ($line !~ m/^Created AMI in /) {
			print("$line\n");
		}
	}

	print <<FOOTER;

Love FreeBSD?  Support this and future releases with a donation to
the FreeBSD Foundation!  https://www.freebsdfoundation.org/donate/

FOOTER

	return(0);
}

&main();
