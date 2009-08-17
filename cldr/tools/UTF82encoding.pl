#!/usr/bin/perl -w

use strict;
use Data::Dumper;

open(FIN, "$ARGV[0]/posix/UTF-8.cm");
my @lines = <FIN>;
chomp(@lines);
close(FIN);

my %cm = ();
foreach my $line (@lines) {
	next if ($line =~ /^#/);
	next if ($line eq "");
	next if ($line !~ /^</);

	my @a = split(" ", $line);
	next if ($#a != 1);

	$a[1] =~ s/\\x//g;
	$cm{$a[1]} = $a[0];
}

print Dumper($cm{"4D"}), "\n";

open(FIN, $ARGV[1]);
@lines = <FIN>;
chomp(@lines);
close(FIN);

foreach my $line (@lines) {
	if ($line =~ /^#/) {
		print "$line\n";
		next;
	}

	my @l = split(//, $line);
	for (my $i = 0; $i <= $#l; $i++) {
		my $hex = sprintf("%X", ord($l[$i]));
		if (defined $cm{$hex}) {
			print $cm{$hex};
			next;
		}

		$hex = sprintf("%X%X", ord($l[$i]), ord($l[$i + 1]));
		if (defined $cm{$hex}) {
			$i += 1;
			print $cm{$hex};
			next;
		}

		$hex = sprintf("%X%X%X",
		    ord($l[$i]), ord($l[$i + 1]), ord($l[$i + 2 ]));
		if (defined $cm{$hex}) {
			$i += 2;
			print $cm{$hex};
			next;
		}

		print "\n--$hex--\n";
	}
	print "\n";

}
