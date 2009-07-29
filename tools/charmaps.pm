#!/usr/local/bin/perl -w

#
# $FreeBSD$
#

use strict;
use XML::Parser;
use Data::Dumper;

my %data = ();
my %d = ();
my $index = -1;

sub get_xmldata {
	my $xmlfile = shift;

	open(FIN, $xmlfile);
	my @xml = <FIN>;
	chomp(@xml);
	close(FIN);

	my $xml = new XML::Parser(Handlers => {
					Start	=> \&h_start,
					End	=> \&h_end,
					Char	=> \&h_char
					});
	$xml->parse(join("", @xml));
	return %d;
}

sub h_start {
	my $expat = shift;
	my $element = shift;
	my @attrs = @_;
	my %attrs = ();


	while ($#attrs >= 0) {
		$attrs{$attrs[0]} = $attrs[1];
		shift(@attrs);
		shift(@attrs);
	}

	$data{element}{++$index} = $element;

	if ($index == 2
	 && $data{element}{1} eq "languages"
	 && $element eq "language") {
		my $name = $attrs{name};
		my $countries = $attrs{countries};
		my $encoding = $attrs{encoding};
		my $family = $attrs{family};
		my $f = defined $attrs{family} ? $attrs{family} : "x";
		my $link = $attrs{link};
		my $fallback = $attrs{fallback};

		$d{L}{$name}{$f}{fallback} = $fallback;
		$d{L}{$name}{$f}{link} = $link;
		$d{L}{$name}{$f}{family} = $family;
		$d{L}{$name}{$f}{encoding} = $encoding;
		$d{L}{$name}{$f}{countries} = $countries;
		foreach my $c (split(" ", $countries)) {
			if (defined $encoding) {
				foreach my $e (split(" ", $encoding)) {
					$d{L}{$name}{$f}{data}{$c}{$e} = undef;
				}
			}
			$d{L}{$name}{$f}{data}{$c}{"UTF-8"} = undef;
		}
		return;
	}

	if ($index == 2
	 && $data{element}{1} eq "translations"
	 && $element eq "translation") {
		if (defined $attrs{hex}) {
			my $k = "<" . $attrs{cldr} . ">";
			my $hs = $attrs{hex};
			$d{T}{$attrs{encoding}}{$k} = "";
			while ($hs ne "") {
				$d{T}{$attrs{encoding}}{$k} .=
					chr(hex(substr($hs, 0, 2)));
				$hs = substr($hs, 2);
			}
		}
		if (defined $attrs{string}) {
			$d{T}{$attrs{encoding}}{"<" . $attrs{cldr} . ">"} =
			    $attrs{string};
		}
		return;
	}

	if ($index == 2
	 && $data{element}{1} eq "alternativemonths"
	 && $element eq "language") {
		my $name = $attrs{name};
		my $countries = $attrs{countries};

		$data{fields}{name} = $name;
		$data{fields}{countries} = $countries;
		$data{fields}{text} = "";

		return;
	}
}

sub h_end {
	my $expat = shift;
	my $element = shift;

	if ($index == "2") {
		if ($data{element}{1} eq "alternativemonths"
		 && $data{element}{2} eq "language") {
			foreach my $c (split(/,/, $data{fields}{countries})) {
				my $m = $data{fields}{text};

				$m =~ s/[\t ]//g;
				$d{AM}{$data{fields}{name}}{$c} = $m;
			}
			$data{fields} = ();
		}
	}

	$index--;
}

sub h_char {
	my $expat = shift;
	my $string = shift;

	if ($index == "2") {
		if ($data{element}{1} eq "alternativemonths"
		 && $data{element}{2} eq "language") {
			$data{fields}{text} .= $string;
		}
	}
}

#use Data::Dumper;
#my %D = get_xmldata();
#print Dumper(%D);
1;
