#!/usr/local/bin/perl -w

use strict;
use XML::Parser;
use Data::Dumper;

my %data = ();
my %d = ();
my $index = -1;

sub get_xmldata {
	open(FIN, "charmaps.xml");
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

	if ($element eq "language") {
		my $name = $attrs{name};
		my $encoding = $attrs{encoding};
		my $countries = $attrs{countries};
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

	if ($element eq "translation") {
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
}

sub h_end {
	my $expat = shift;
	my $element = shift;
	$index--;
}

sub h_char {
	my $expat = shift;
	my $string = shift;
}

#use Data::Dumper;
#my %D = get_xmldata();
#print Dumper(%D);
1;
