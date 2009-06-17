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

our $already;

our $branch = "head";
our $target = ".";
our @revs;

our $svn_path;
our $svn_url;
our $svn_root;
our $svn_branch;

sub svn_check($;$) {
    my ($cond, $msg) = @_;

    die(($msg || "something is rotten in the state of subversion") . "\n")
	unless $cond;
}

sub svn_do(@) {
    my @argv = @_;

    print(join(' ', "svn", @argv), "\n");
    system("svn", @argv);
}

sub svn_info() {
    local *PIPE;

    open(PIPE, "-|", "svn", "info", $target)
	or die("fmerge: could not run svn\n");
    while (<PIPE>) {
	chomp();
	my ($key, $value) = split(/:\s+/, $_, 2);
	next unless $key && $value;
	if ($key eq "Path") {
	    svn_check($value eq $target);
	} elsif ($key eq "URL") {
	    $svn_url = $value;
	} elsif ($key eq "Repository Root") {
	    $svn_root = $value;
	}
    }
    close(PIPE);

    svn_check($svn_url =~ m@^\Q$svn_root\E(/.*)$@);
    $svn_path = $1;
    svn_check($svn_path =~ s@^/(\w+/\d+(?:\.\d+)*)/?@@);
    $svn_branch = $1;
}

sub fmerge() {
    foreach my $rev (@revs) {
	my ($m, $n) = @{$rev};
	my @argv = ("merge");
	if ($already) {
	    push(@argv, "--record-only");
	}
	push(@argv,
	     "-r$m:$n",
	     "$svn_root/$branch/$svn_path",
	     $target);
	svn_do(@argv);
    }
}

sub usage() {

    print(STDERR "usage: fmerge REVISIONS [from BRANCH] [into DIR]\n");
    exit 1;
}

MAIN:{
    if (@ARGV < 1) {
	usage();
    }
    if ($ARGV[0] eq "already") {
	$already = 1;
	shift;
    }
    if ($ARGV[0] eq "all") {
	shift;
    } else {
	while (@ARGV && $ARGV[0] =~ m/^r?\d+(,r?\d+)*$/) {
	    foreach my $rev (split(',', $ARGV[0])) {
		if ($rev =~ m/^[cr]?(\d+)$/) {
		    push(@revs, [ $1 - 1, $1 ]);
		} elsif ($rev =~ m/^[cr]?-(\d+)$/) {
		    push(@revs, [ $1, $1 - 1 ]);
		} elsif ($rev =~ m/^r?(\d+)[-:](\d+)$/) {
		    push(@revs, [ $1, $2 ]);
		} else {
		    usage();
		}
	    }
	    shift;
	}
    }

    if (@ARGV > 0) {
	if (@ARGV < 2) {
	    usage();
	}
	if ($ARGV[0] ne "from") {
	    usage();
	}
	shift;
	$branch = $ARGV[0];
	shift;
    }

    if (@ARGV > 0) {
	if (@ARGV < 2) {
	    usage();
	}
	if ($ARGV[0] ne "into") {
	    usage();
	}
	shift;
	$target = $ARGV[0];
	shift;
	if (!-d $target) {
	    usage();
	}
    }

    if (@ARGV > 0) {
	usage();
    }

    svn_info();
    fmerge();
}

1;
