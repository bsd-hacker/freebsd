#!/usr/bin/perl -w
#-
# Copyright (c) 2009 Dag-Erling Coïdan Smørgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

use strict;
use warnings;
use FileHandle;
use IPC::Open2;

my %struct;

# sub read_machdep($)
# {
#     my ($fn) = @_;

#     open(my $fh, "<", $fn)
# 	or die("$fn: $!\n");
    
#     close($fh);
# }

sub read_config($)
{
    my ($fn) = @_;

    open(my $fh, "<", $fn)
	or die("$fn: $!\n");
    local $/;
    my $config = <$fh>;
    close($fh);

    $config =~ s/\/\*.*?\*\// /gs;
    $config =~ s/\/\/.*$//gm;
    my @cpp;
    foreach (split("\n", $config)) {
	s/\s+$//;
	s/\s+/ /g;
	next unless m/./;
	if (m/^#/) {
	    push(@cpp, $_);
	} elsif (m/^struct ([A-Za-z_][0-9A-Za-z_]*);$/) {
	    die("struct $1 specified more than once\n")
		if (exists($struct{$1}));
	    $struct{$1} = [ @cpp ];
	} else {
	    die("syntax error in $fn: $_\n");
	}
    }
}

sub parse_struct($@)
{
    my ($struct, @cpp) = @_;

    $SIG{'PIPE'} = sub { die("SIGPIPE from cpp\n"); };
    my $pid = open2(my $out, my $in, "cpp");
    foreach (@cpp) {
	print($in "$_\n");
    }
    close($in);
    local $/;
    my $c = <$out>;
    close($out);
    waitpid($pid, 0);
    my $status = $? >> 8;
    die("exit code $status from cpp\n")
	if ($status != 0);
    $c =~ s/^#.*$//gm;
    $c =~ s/\s+/ /gs;
    while ($c =~ m/\G.*?\btypedef\s+((?:(?:struct|union)(?:\s+[A-Za-z_][0-9A-Za-z_]*|\s*{.*?})|[A-Za-z_][0-9A-Za-z_]*)(?:\s*\*)?)\s*([A-Za-z_][0-9A-Za-z_]*);/g) {
	print("typedef $1 $2;\n");
    }
    $c =~ m/\bstruct $struct ?({.*?}) ?;/
	or die("struct $struct not found in cpp output\n");
    my $struct_def = $1;
    print("struct $struct $struct_def;\n");
}

MAIN:{
    foreach my $arg (@ARGV) {
	read_config($arg);
	foreach (sort(keys(%struct))) {
	    parse_struct($_, @{$struct{$_}});
	}
    }
}

1;
