#!/usr/bin/perl -wC

use strict;
use XML::Parser;
use Text::Iconv;
use Tie::IxHash;
use Data::Dumper;
use Digest::SHA qw(sha1_hex);
require "charmaps.pm";

if ($#ARGV < 2) {
	print "Usage: $0 <cldrdir> <charmaps> <type> [la_CC]\n";
	exit(1);
}

my $DEFENCODING = "UTF-8";
my $DIR = shift(@ARGV);
my $CHARMAPS = shift(@ARGV);
my $TYPE = shift(@ARGV);
my $doonly = shift(@ARGV);
my @filter = ();

my %convertors = ();

my %values = ();
my %hashtable = ();
my %languages = ();
my %translations = ();
get_languages();

my %cm = ();
get_utf8map();

my %keys = ();
tie(%keys, "Tie::IxHash");
tie(%hashtable, "Tie::IxHash");

my %FILESNAMES = (
	"monetdef"	=> "LC_MONETARY",
	"timedef"	=> "LC_TIME",
	"msgdef"	=> "LC_MESSAGES",
	"numericdef"	=> "LC_NUMERIC"
);

my %callback = (
	mdorder => \&callback_mdorder,
);

my %DESC = (

	# numericdef
	"decimal_point"	=> "decimal_point",
	"thousands_sep"	=> "thousands_sep",
	"grouping"	=> "grouping",

	# monetdef
	"int_curr_symbol"	=> "int_curr_symbol (last character always " .
				   "SPACE)",
	"currency_symbol"	=> "currency_symbol",
	"mon_decimal_point"	=> "mon_decimal_point",
	"mon_thousands_sep"	=> "mon_thousands_sep",
	"mon_grouping"		=> "mon_grouping",
	"positive_sign"		=> "positive_sign",
	"negative_sign"		=> "negative_sign",
	"int_frac_digits"	=> "int_frac_digits",
	"frac_digits"		=> "frac_digits",
	"p_cs_precedes"		=> "p_cs_precedes",
	"p_sep_by_space"	=> "p_sep_by_space",
	"n_cs_precedes"		=> "n_cs_precedes",
	"n_sep_by_space"	=> "n_sep_by_space",
	"p_sign_posn"		=> "p_sign_posn",
	"n_sign_posn"		=> "n_sign_posn",

	# msgdef
	"yesexpr"	=> "yesexpr",
	"noexpr"	=> "noexpr",
	"yesstr"	=> "yesstr",
	"nostr"		=> "nostr",

	# timedef
	"abmon"		=> "Short month names",
	"mon"		=> "Long month names (as in a date)",
	"abday"		=> "Short weekday names",
	"day"		=> "Long weekday names",
	"t_fmt"		=> "X_fmt",
	"d_fmt"		=> "x_fmt",
	"XXX"		=> "c_fmt",
	"am_pm"		=> "AM/PM",
	"d_t_fmt"	=> "date_fmt",
	"mon2"		=> "Long month names (without case ending)",
	"md_order"	=> "md_order",
	"t_fmt_ampm"	=> "ampm_fmt",

);

if ($TYPE eq "numericdef") {
	%keys = (
	    "decimal_point"	=> "s",
	    "thousands_sep"	=> "s",
	    "grouping"		=> "ai",
	);
	get_fields();
	print_fields();
	make_makefile();
}

if ($TYPE eq "monetdef") {
	%keys = (
	    "int_curr_symbol"	=> "s",
	    "currency_symbol"	=> "s",
	    "mon_decimal_point"	=> "s",
	    "mon_thousands_sep"	=> "s",
	    "mon_grouping"	=> "ai",
	    "positive_sign"	=> "s",
	    "negative_sign"	=> "s",
	    "int_frac_digits"	=> "i",
	    "frac_digits"	=> "i",
	    "p_cs_precedes"	=> "i",
	    "p_sep_by_space"	=> "i",
	    "n_cs_precedes"	=> "i",
	    "n_sep_by_space"	=> "i",
	    "p_sign_posn"	=> "i",
	    "n_sign_posn"	=> "i"
	);
	get_fields();
	print_fields();
	make_makefile();
}

if ($TYPE eq "msgdef") {
	%keys = (
	    "yesexpr"		=> "s",
	    "noexpr"		=> "s",
	    "yesstr"		=> "s",
	    "nostr"		=> "s"
	);
	get_fields();
	print_fields();
	make_makefile();
}

if ($TYPE eq "timedef") {
	%keys = (
	    "abmon"		=> "as",
	    "mon"		=> "as",
	    "abday"		=> "as",
	    "day"		=> "as",
	    "t_fmt"		=> "s",
	    "d_fmt"		=> "s",
	    "XXX"		=> "s",
	    "am_pm"		=> "as",
	    "d_fmt"		=> "s",
	    "d_t_fmt"		=> "s",
	    "mon2"		=> ">mon",		# repeat them for now
	    "md_order"		=> "<mdorder<d_fmt<s",
	    "t_fmt_ampm"	=> "s",
	);
	get_fields();
	print_fields();
	make_makefile();
}

sub callback_mdorder {
	my $s = shift;
	return undef if (!defined $s);
	$s =~ s/[^dm]//g;
	return $s;
};

############################

sub get_utf8map {
	open(FIN, "$DIR/posix/$DEFENCODING.cm");
	my @lines = <FIN>;
	close(FIN);
	chomp(@lines);
	my $incharmap = 0;
	foreach my $l (@lines) {
		$l =~ s/\r//;
		next if ($l =~ /^\#/);
		next if ($l eq "");
		if ($l eq "CHARMAP") {
			$incharmap = 1;
			next;
		}
		next if (!$incharmap);
		last if ($l eq "END CHARMAP");
		$l =~ /^([^\s]+)\s+(.*)/;
		my $k = $1;
		my $v = $2;
		$v =~ s/\\x//g;
		$cm{$k} = $v;
	}
}

sub get_languages {
	my %data = get_xmldata($CHARMAPS);
	%languages = %{$data{L}}; 
	%translations = %{$data{T}}; 

	return if (!defined $doonly);

	my @a = split(/_/, $doonly);
	if ($#a == 1) {
		$filter[0] = $a[0];
		$filter[1] = "x";
		$filter[2] = $a[1];
	} elsif ($#a == 2) {
		$filter[0] = $a[0];
		$filter[1] = $a[1];
		$filter[2] = $a[2];
	}

	print Dumper(@filter);
	return;
}

sub get_fields {
	foreach my $l (sort keys(%languages)) {
	foreach my $f (sort keys(%{$languages{$l}})) {
	foreach my $c (sort keys(%{$languages{$l}{$f}{data}})) {
		next if ($#filter == 2 && ($filter[0] ne $l
		    || $filter[1] ne $f || $filter[2] ne $c));

		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = 0;	# unread
		my $file;
		$file = $l . "_";
		$file .= $f . "_" if ($f ne "x");
		$file .= $c;
		if (!open(FIN, "$DIR/posix/$file.$DEFENCODING.src")) {
			if (!defined $languages{$l}{$f}{fallback}) {
				print STDERR
				    "Cannot open $file.$DEFENCODING.src\n";
				next;
			}
			$file = $languages{$l}{$f}{fallback};
			if (!open(FIN, "$DIR/posix/$file.$DEFENCODING.src")) {
				print STDERR
				    "Cannot open fallback " .
				    "$file.$DEFENCODING.src\n";
				next;
			}
		}
		print "Reading from $file.$DEFENCODING.src for ${l}_${f}_${c}\n";
		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = 1;	# read
		my @lines = <FIN>;
		chomp(@lines);
		close(FIN);
		my $continue = 0;
		foreach my $k (keys(%keys)) {
			foreach my $line (@lines) {
				$line =~ s/\r//;
				next if (!$continue && $line !~ /^$k\s/);
				if ($continue) {
					$line =~ s/^\s+//;
				} else {
					$line =~ s/^$k\s+//;
				}

				$values{$l}{$c}{$k} = ""
					if (!defined $values{$l}{$c}{$k});

				$continue = ($line =~ /\/$/);
				$line =~ s/\/$// if ($continue);
				$values{$l}{$c}{$k} .= $line;

				last if (!$continue);
			}
		}
	}
	}
	}
}

sub decodecldr {
	my $s = shift;
	my $v = $cm{$s};

	return pack("C", hex($v)) if (length($v) == 2);
	return pack("CC", hex(substr($v, 0, 2)), hex(substr($v, 2, 2)))
		if (length($v) == 4);
	return pack("CCC", hex(substr($v, 0, 2)), hex(substr($v, 2, 2)),
	    hex(substr($v, 4, 2))) if (length($v) == 6);
	return "length = " . length($v);
}

sub translate {
	my $enc = shift;
	my $v = shift;

	return $translations{$enc}{$v} if (defined $translations{$enc}{$v});
	return undef;
}

sub print_fields {
	foreach my $l (sort keys(%languages)) {
	foreach my $f (sort keys(%{$languages{$l}})) {
	foreach my $c (sort keys(%{$languages{$l}{$f}{data}})) {
		next if ($#filter == 2 && ($filter[0] ne $l
		    || $filter[1] ne $f || $filter[2] ne $c));
		foreach my $enc (sort keys(%{$languages{$l}{$f}{data}{$c}})) {
			if ($languages{$l}{$f}{data}{$c}{$DEFENCODING} eq "0") {
				print "Skipping ${l}_" .
				    ($f eq "x" ? "" : "${f}_") .
				    "${c} - not read\n";
				next;
			}
			my $file = $l;
			$file .= "_" . $f if ($f ne "x");
			$file .= "_" . $c;
			print "Writing to $file in $enc\n";

			eval {
				$convertors{$enc} =
				    Text::Iconv->new($DEFENCODING, $enc);
			} if (!defined $convertors{$enc});
			if (!defined $convertors{$enc}) {
				print "Failed! Cannot convert between " .
				    "$DEFENCODING and $enc.\n";
				next;
			};

			open(FOUT, ">$TYPE/$file.$enc.new");
			my $okay = 1;
			my $output = "";
			print FOUT <<EOF;
# \$FreeBSD\$
#
# Warning: Do not edit. This file is automatically generated from the
# tools in /usr/src/tools/tools/locale. The data is obtained from the
# CLDR project, obtained from http://cldr.unicode.org/
#
# ${l}_$c in $enc
#
# -----------------------------------------------------------------------------
EOF
			foreach my $k (keys(%keys)) {
				my $f = $keys{$k};

				die("Unknown $k in \%DESC")
					if (!defined $DESC{$k});

				$output .= "#\n# $DESC{$k}\n";

				if ($f =~ /^>/) {
					$k = substr($f, 1);
					$f = $keys{$k};
				}
				if ($f =~ /^\</) {
					my @a = split(/\</, substr($f, 1));
					my $rv =
					    &{$callback{$a[0]}}($values{$l}{$c}{$a[1]});
					$values{$l}{$c}{$k} = $rv;
					$f = $a[2];
				}

				my $v = $values{$l}{$c}{$k};
				$v = "undef" if (!defined $v);

				if ($f eq "i") {
					$output .= "$v\n";
					next;
				}
				if ($f eq "ai") {
					$output .= "$v\n";
					next;
				}
				if ($f eq "s") {
					$v =~ s/^"//;
					$v =~ s/"$//;
					my $cm = "";
					while ($v =~ /^(.*?)(<.*?>)(.*)/) {
						$cm = $2;
						$v = $1 . decodecldr($2) . $3;
					}
					my $fv =
					    $convertors{$enc}->convert("$v");
					$fv = translate($enc, $cm)
						if (!defined $fv);
					if (!defined $fv) {
						print STDERR 
						    "Could not convert $k " .
						    "($cm) from $DEFENCODING " .
						    "to $enc\n";
						$okay = 0;
						next;
					}
					$output .= "$fv\n";
					next;
				}
				if ($f eq "as") {
					foreach my $v (split(/;/, $v)) {
						$v =~ s/^"//;
						$v =~ s/"$//;
						my $cm = "";
						while ($v =~ /^(.*?)(<.*?>)(.*)/) {
							$cm = $2;
							$v = $1 .
							    decodecldr($2) . $3;
						}
						my $fv =
						    $convertors{$enc}->convert("$v");
						$fv = translate($enc, $cm)
							if (!defined $fv);
						if (!defined $fv) {
							print STDERR
							    "Could not " .
							    "convert $k ($cm)" .
							    " from " .
							    "$DEFENCODING to " .
							    "$enc\n";
							$okay = 0;
							next;
						}
						$output .= "$fv\n";
					}
					next;
				}

				die("$k is '$f'");

			}

			$languages{$l}{$f}{data}{$c}{$enc} = sha1_hex($output);
			$hashtable{sha1_hex($output)}{"${l}_${f}_${c}.$enc"} = 1;
			print FOUT "$output# EOF\n";
			close(FOUT);

			if ($okay) {
				rename("$TYPE/$file.$enc.new",
				    "$TYPE/$file.$enc.src");
			} else {
				rename("$TYPE/$file.$enc.new",
				    "$TYPE/$file.$enc.failed");
			}
		}
	}
	}
	}
}

sub make_makefile {
	return if ($#filter > -1);
	print "Creating Makefile for $TYPE\n";
	open(FOUT, ">$TYPE/Makefile");
	print FOUT <<EOF;
#
# \$FreeBSD\$
#
# Warning: Do not edit. This file is automatically generated from the
# tools in /usr/src/tools/tools/locale.
# 

LOCALEDIR=	/usr/share/locale
FILESNAME=	$FILESNAMES{$TYPE}
.SUFFIXES:	.src .out

.src.out:
	grep -v '^\#' < \${.IMPSRC} > \${.TARGET}

EOF

	foreach my $hash (keys(%hashtable)) {
		my @files = sort(keys(%{$hashtable{$hash}}));
		if ($#files > 0) {
			my $link = shift(@files);
			$link =~ s/_x_/_/;	# strip family if none there
			foreach my $file (@files) {
				my @a = split(/_/, $file);
				my @b = split(/\./, $a[-1]);
				$file =~ s/_x_/_/;
				print FOUT "SAME+=\t\t$link:$file\t#hash\n";
				undef($languages{$a[0]}{$a[1]}{data}{$b[0]}{$b[1]});
			}
		}
	}

	foreach my $l (sort keys(%languages)) {
	foreach my $f (sort keys(%{$languages{$l}})) {
	foreach my $c (sort keys(%{$languages{$l}{$f}{data}})) {
		next if ($#filter == 2 && ($filter[0] ne $l
		    || $filter[1] ne $f || $filter[2] ne $c));
		foreach my $e (sort keys(%{$languages{$l}{$f}{data}{$c}})) {
			my $file = $l . "_";
			$file .= $f . "_" if ($f ne "x");
			$file .= $c;
			next if (!defined $languages{$l}{$f}{data}{$c}{$e});
			print FOUT "LOCALES+=\t$file.$e\n";
		}

		if (defined $languages{$l}{$f}{link}) {
			foreach my $e (sort keys(%{$languages{$l}{$f}{data}{$c}})) {
				my $file = $l . "_";
				$file .= $f . "_" if ($f ne "x");
				$file .= $c;
				print FOUT "SAME+=\t\t$file.$e:$languages{$l}{$f}{link}.$e\t# legacy\n";
				
			}
			
		}

	}
	}
	}

	print FOUT <<EOF;

FILES=		\${LOCALES:S/\$/.out/}
CLEANFILES=	\${FILES}

.for f in \${SAME}
SYMLINKS+=	../\${f:C/:.*\$//}/\${FILESNAME} \${LOCALEDIR}/\${f:C/^.*://}
.endfor

.for f in \${LOCALES}
FILESDIR_\${f}.out= \${LOCALEDIR}/\${f}
.endfor


src:
	./cldr2def.pl /home/edwin/cldr/1.7.0/ charmaps.xml timedef nl_NL

.include <bsd.prog.mk>
EOF

	close(FOUT);
}
