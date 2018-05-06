#!/usr/bin/env perl
#
# $Id$
#

my $file = "$ARGV[0]";
my $haveuid = 0;
my $havekey = 0;
my $uid = '';

sub usage() {
    print "Usage: $ARGV[0] /path/to/ldap/data\n";
    exit (1);
}

sub main() {
    if (!$ARGV[0]) {
	&usage();
    }
    open(FILE, $file) or die("Could not open $file\n");
    while(<FILE>) {
	chomp($_);

	# Skip commented lines.
	if ($_ =~ m/^#/) {
	    next;
	}

	# Skip empty lines, reset vars.
	if ($_ =~ m/^$/) {
	    $haveuid = 0;
	    $uid = '';
	    $havekey = 0;
	    next;
	}

	# Found the uid field.  Make sure it is not empty, then set
	# haveuid=1.
	if ($_ =~ m/^uid: /) {
	    $_ =~ s/^uid: //;
	    # The one unfortunate account *with* an ssh key.
	    if ($_ =~ m/backup/) {
		next;
	    }
	    $uid = $_;
	    $haveuid = 1;
	}

	# No need to search for a key if haveuid=0.
	if ($haveuid eq 1) {
	    # Have the key.
	    if ($_ =~ m/^sshPublicKey::? /) {
		$_ =~ s/^sshPublicKey::? //;
		# It should not happen, but if a key datafield exists
		# without a key, bail.
		if ($_ =~ m//) {
		    $haveuid = 0;
		    next;
		}
		# Great.  We have found a key for the UID.  Since they
		# have login access, they can vote.  Good for them.
		$havekey = 1;
		print "$uid\n";
		$haveuid = 0;
	    }
	}
    }
    close(FILE);
}

&main();

