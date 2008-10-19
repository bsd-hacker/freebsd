#!/usr/bin/perl -w

use XML::Parser;
use Data::Dumper;
use strict;

my @tree = ();
my @values = ();
my $treeindex = -1;

my %branches = ();
my %softwares = ();
my $software = "";
my $mfcbranch = "";
my $swversion = "";

my %releaseent = ();

sub xml_start {
	my $expat = shift;
	my $element = shift;

	$tree[++$treeindex] = $element;
	while (defined (my $attribute = shift)) {
		$values[$treeindex]{$attribute} = shift;
	}

	if ($element eq "software") {
		$software = $values[$treeindex]{name};
	}
	if ($element eq "version") {
		$softwares{$software}{versions}{$values[$treeindex]{version}} = {};
		$swversion = $values[$treeindex]{version};
	}
	if ($element eq "mfc") {
		$mfcbranch = $values[$treeindex]{branch};
	}

}

sub xml_end {
	my $expat = shift;
	my $element = shift;

	$values[$treeindex] = ();
	$treeindex--;
}

sub xml_char {
	my $expat = shift;
	my $value = shift;

	if ($tree[0] eq "freebsd") {
		return if ($treeindex == 0);

		if ($tree[1] eq "branches") {
			return if ($treeindex == 1);

			if ($tree[2] eq "branch") {
				$branches{$values[$treeindex]{name}} = $value;
				return;
			}

			return;
		}

		if ($tree[1] eq "softwares") {
			return if ($treeindex == 1);

			if ($tree[2] eq "software") {
				return if ($treeindex == 2);

				if ($tree[3] eq "desc") {
					$softwares{$software}{desc} = ""
						if (!defined $softwares{$software}{desc});
					$softwares{$software}{desc} .= $value;
					return;
				}

				if ($tree[3] eq "versions") {
					return if ($treeindex == 3);

					if ($tree[4] eq "version") {
						return if ($treeindex == 4);

						if ($tree[5] eq "import") {
							$softwares{$software}{versions}{$swversion}{import} = $value;
							return;
						}
						if ($tree[5] eq "mfv") {
							$softwares{$software}{versions}{$swversion}{mfv} = $value;
							return;
						}
						if ($tree[5] eq "desc") {
							$softwares{$software}{versions}{$swversion}{desc} = ""
								if (!defined $softwares{$software}{versions}{$swversion}{desc});
							$softwares{$software}{versions}{$swversion}{desc} .= $value;
							return;
						}
						if ($tree[5] eq "mfc") {
							$softwares{$software}{versions}{$swversion}{mfc}{$mfcbranch} = $value;
							return;
						}

					}
				}
			}
		}
	}

}

my $p = new XML::Parser(
	Handlers => {
		Start   => \&xml_start,
		End     => \&xml_end,
		Char    => \&xml_char,
	});
$p->parsefile("../../../../../contrib.xml");

{
	my %r = (
		"release.current"	=> 1,
		"release.next"		=> 1,
		"release.prev"		=> 1,
		"release.branch"	=> 1,
	);
	open(FIN, "../../share/sgml/release.ent");
	my @lines = <FIN>;
	close(FIN);
	chomp(@lines);

	foreach my $line (@lines) {
		if ($line =~ /<!ENTITY ([^ ]+) "([^\-]+).*">/) {
			next if (!defined $r{$1});
			$releaseent{$1} = $2;
		}
	}
}

#print Dumper(%branches);
#print Dumper(%releaseent);
#print Dumper(%softwares);

#
# If we are in -current, then release.current doesn't exist yet.
# In that case copy all MFVs into MFC{release.branch}. The date
# of the new MFC is the date of the release.prev.
#
# release.current is then assigned with release.branch.
#
# the creation date of branches{release.current} will be
# branches{release.prev}
#
if (!defined $branches{$releaseent{"release.current"}}) {
	foreach my $sw (keys(%softwares)) {
		foreach my $vs (keys(%{$softwares{$sw}{versions}})) {
			next if (!defined $softwares{$sw}{versions}{$vs}{mfv});
			$softwares{$sw}{versions}{$vs}{mfc}{$releaseent{"release.branch"}} = $softwares{$sw}{versions}{$vs}{mfv};
		}
	}

	$releaseent{"release.current"} = $releaseent{"release.branch"};
	$branches{$releaseent{"release.current"}} =
		$branches{$releaseent{"release.prev"}}

}

#
# Changes in software versions are determined as follows:
#
# For version N.M and M != 0, compare against branches{N.(M-1)}
# For version N.0, compare against branches{(N-1)}
# For version N, compare against branches{(N-1)}
#
my $thisversion = $releaseent{"release.current"};
my $prevversion = "";

# XXX - This fails for 5.2.1
if ($thisversion =~ /^(\d+)\.(\d+)/) {
	my $major = $1;
	my $minor = $2;
	if ($minor eq "0") {
		$prevversion = $major - 1;
	} else {
		$prevversion = sprintf("%d.%d", $major, $minor - 1);
	}
} elsif ($thisversion =~ /^(\d+)$/) {
	$prevversion = $1 - 1;
}

print "$thisversion - $prevversion\n";
print "$branches{$thisversion} - $branches{$prevversion}\n";
#print Dumper(%branches);
#print Dumper(%releaseent);
#print Dumper(%softwares);
