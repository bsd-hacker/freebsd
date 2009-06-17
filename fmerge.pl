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
our $debug;
our $pretend;

our $branch = "head";
our $target = ".";
our @revs;

our $svn_path;
our $svn_url;
our $svn_root;
our $svn_branch;

sub info(@) {
    print(STDOUT join(' ', @_), "\n");
}

sub debug(@) {
    info(@_)
	if $debug;
}

sub svn_check($;$) {
    my ($cond, $msg) = @_;
    die(($msg || 'something is rotten in the state of subversion') . "\n")
	unless $cond;
}

sub svn_do(@) {
    my @argv = @_;
    info('svn', @argv);
    system('svn', @argv)
	unless $pretend;
}

sub svn_merge(@) {
    unshift(@_, '--record-only')
	if $already;
    unshift(@_, 'merge');
    goto &svn_do;
}

sub svn_catch(@) {
    my (@argv) = @_;

    debug('svn', @argv);
    open(my $fh, '-|', 'svn', @argv)
	or die("fmerge: could not run svn\n");
    return $fh;
}

sub examine() {
    my $fh = svn_catch("info", $target);
    while (<$fh>) {
	chomp();
	my ($key, $value) = split(/:\s+/, $_, 2);
	next unless $key && $value;
	if ($key eq 'Path') {
	    debug("'$value' eq '$target'?");
	    svn_check($value eq $target);
	} elsif ($key eq 'URL') {
	    $svn_url = $value;
	} elsif ($key eq 'Repository Root') {
	    $svn_root = $value;
	}
    }
    close($fh);

    svn_check($svn_url =~ m@^\Q$svn_root\E(/.*)$@);
    $svn_path = $1;

    $fh = svn_catch('propget', 'svn:mergeinfo', $target);
    while (<$fh>) {
	chomp();
	debug("'$_' =~ m\@\Q/$branch\E((?:/[\\w.-]+)*):\@");
	next unless m@\Q/$branch\E((?:/[\w.-]+)*):@;
	my $subdir = $1;
	debug("'$svn_path' =~ m\@^((?:/[\\w.-]+)+)\Q$subdir\E\$\@");
	next unless $svn_path =~ m@^((?:/[\w.-]+)+)\Q$subdir\E$@;
	$svn_path = $subdir;
	$svn_branch = $1;
	last;
    }
    close($fh);
    if (!$svn_branch) {
	# try to guess a stable / releng / release branch
	debug("'$svn_path' =~ s\@^/([\\w+.-]/\\d+(?:\\.\\d+)*)/?\@\@");
	$svn_path =~ s@^/(\w+/\d+(?:\.\d+)*)/?@@;
	$svn_branch = $1;
    }
    svn_check($svn_branch);
    debug("svn_branch = '$svn_branch'");
    debug("svn_path = '$svn_path'");
}

sub fmerge() {
    if (!@revs) {
	svn_merge("$svn_root/$branch/$svn_path", $target);
    }
    foreach my $rev (@revs) {
	my ($m, $n) = @{$rev};
	svn_merge("-r$m:$n", "$svn_root/$branch/$svn_path", $target);
    }
}

sub usage() {

    print(STDERR "usage: fmerge REVISIONS [from BRANCH] [into DIR]\n");
    exit 1;
}

MAIN:{
    while (@ARGV) {
	if ($ARGV[0] eq 'already') {
	    $already++;
	} elsif ($ARGV[0] eq 'debug') {
	    $debug++;
	} elsif ($ARGV[0] eq 'pretend') {
	    $pretend++;
	} else {
	    last;
	}
	shift;
    }
    if (@ARGV < 1) {
	usage();
    }
    if ($ARGV[0] eq 'all') {
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
	if ($ARGV[0] ne 'from') {
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
	if ($ARGV[0] ne 'into') {
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

    examine();
    fmerge();
}

1;
