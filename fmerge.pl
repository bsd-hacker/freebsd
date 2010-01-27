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

our $src_branch = "head";	# where we merge from
our $src_path;			# path relative to source branch
our $src_url;			# source URL
our $tgt_branch;		# where we merge to
our $tgt_path;			# path relative to target branch
our $tgt_dir = ".";		# target directory
our $tgt_url;			# target URL

our %revs = (0 => 0);
our @ranges;

our $svn_path;			# path relative to branch
our $svn_root;			# Repository root

sub info(@) {
    print(STDERR join(' ', @_), "\n");
}

sub debug(@) {
    info(@_)
	if $debug;
}

sub svn_check($@) {
    my ($cond, @msg) = @_;
    if (!$cond) {
	info(@msg);
	exit(1);
    }
}

sub svn_do(@) {
    my @argv = @_;
    unshift(@argv, '--dry-run')
	if $pretend;
    info('svn', @argv);
    my $pid = fork();
    if ($pid == -1) {
	die("fork(): $!\n");
    } elsif ($pid == 0) {
	exec('svn', @argv);
	die("exec(): $!\n");
    }
    waitpid($pid, 0);
    if ($? & 128) {
	info("svn died with signal", $? & 128);
	kill($? & 128, $$);
    } elsif ($?) {
	info("svn returned error status", $? >> 8);
	exit(1);
    }
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
    my $fh = svn_catch("info", $tgt_dir);
    while (<$fh>) {
	chomp();
	my ($key, $value) = split(/:\s+/, $_, 2);
	next unless $key && $value;
	if ($key eq 'Path') {
	    svn_check($value eq $tgt_dir, "path mismatch: $value != $tgt_dir");
	} elsif ($key eq 'URL') {
	    $tgt_url = $value;
	} elsif ($key eq 'Repository Root') {
	    $svn_root = $value;
	}
    }
    close($fh);

    svn_check($tgt_url =~ m@^\Q$svn_root\E(/.*)$@, "invalid svn URL: $tgt_url");
    $svn_path = $1;

    debug("guessing merge source / target directory");
    $fh = svn_catch('propget', 'svn:mergeinfo', $tgt_dir);
    while (<$fh>) {
	chomp();
	debug("'$_' =~ m\@^\Q/$src_branch\E((?:/[\\w.-]+)*):\@");
	next unless m@^\Q/$src_branch\E((?:/[\w.-]+)*):@;
	my $subdir = $1;
	debug("'$svn_path' =~ m\@^((?:/[\\w.-]+)+)\Q$subdir\E\$\@");
	next unless $svn_path =~ m@^((?:/[\w.-]+)+)\Q$subdir\E$@;
	$svn_path = $subdir;
	$tgt_branch = $1;
	last;
    }
    close($fh);

    if (!$tgt_branch) {
	# try to guess a stable / releng / release branch
	debug("guessing target branch");
	debug("'$svn_path' =~ s\@^/([\\w+.-]/\\d+(?:\\.\\d+)*)/?\@\@");
	$svn_path =~ s@^/(head|\w+/\d+(?:\.\d+)*)/?@@;
	$tgt_branch = $1;
    }
    svn_check($tgt_branch, "unable to figure out source branch");
    debug("tgt_branch = '$tgt_branch'");
    debug("svn_path = '$svn_path'");
}

sub addrevs($$) {
    my ($m, $n) = @_;
    debug("adding range r$m:$n");
    if ($m > $n) {
	for (my $i = $m; $i > $n; --$i) {
	    $revs{$i} = -1;
	}
    } else {
	for (my $i = $m + 1; $i <= $n; ++$i)  {
	    $revs{$i} = +1;
	}
    }
}

sub revs2ranges() {
    my ($m, $n);
    # process in reverse, 0 acts as a sentinel
    foreach my $i (sort({ $b <=> $a } keys(%revs)), 0) {
	if (!$m) {
	    $m = $n = $i;
	    next;
	} elsif ($i == $m - 1 && $revs{$m} == +1 && $revs{$i} == +1) {
	    $m = $i;
	    next;
	} elsif ($i == $m + 1 && $revs{$m} == -1 && $revs{$i} == -1) {
	    $m = $i;
	    next;
	} else {
	    if ($revs{$m} == +1) {
		unshift(@ranges, [ $m - 1, $n ]);
	    } elsif ($revs{$m} == -1) {
		unshift(@ranges, [ $n, $m - 1 ]);
	    }
	    $m = $n = $i;
	}
    }
    debug(join("\n  ", "ranges:", map { "r$_->[0]:$_->[1]" } @ranges));
}

sub printranges($) {
    my ($fh) = @_;
    my @print;
    foreach my $range (@ranges) {
	my ($m, $n) = @{$range};
	if ($n == $m + 1) {
	    push(@print, $n);
	} elsif ($n == $m - 1) {
	    push(@print, "-$m"); 
	} else {
	    push(@print, "$m:$n");
	}
    }
    print($fh "merging ", join(', ', @print), "\n")
	if @print;
}

sub fmerge() {
    if (!@ranges) {
	svn_merge("$svn_root/$src_branch/$svn_path", $tgt_dir);
    }
    foreach my $range (@ranges) {
	my ($m, $n) = @{$range};
	my $spec;
	if ($n == $m + 1) {
	    $spec = "-c$n";
	} elsif ($n == $m - 1) {
	    $spec = "-c-$n";
	} else {
	    $spec = "-r$m:$n";
	}
	svn_merge($spec, "$svn_root/$src_branch/$svn_path", $tgt_dir);
    }
}

sub usage() {

    print(STDERR "usage: fmerge REVISIONS [from BRANCH] [into DIR]\n");
    exit 1;
}

MAIN:{
    while (@ARGV) {
	if ($ARGV[0] eq 'already') {
	    shift;
	    $already++;
	} elsif ($ARGV[0] eq 'debug') {
	    shift;
	    $debug++;
	} elsif ($ARGV[0] eq 'pretend') {
	    shift;
	    $pretend++;
	} else {
	    last;
	}
    }
    if (@ARGV < 1) {
	usage();
    }
    if ($ARGV[0] eq 'all') {
	shift;
    } else {
	while (@ARGV && $ARGV[0] =~ m/^[cr]?\d+([-:][cr]?\d+)?(,[cr]?\d+([-:][cr]?\d+)?)*$/) {
	    foreach my $rev (split(',', $ARGV[0])) {
		if ($rev =~ m/^[cr]?(\d+)$/) {
		    addrevs($1 - 1, $1);
		} elsif ($rev =~ m/^[cr]?-(\d+)$/) {
		    addrevs($1, $1 - 1);
		} elsif ($rev =~ m/^[cr]?(\d+)[-:][cr]?(\d+)$/) {
		    if ($1 < $2) {
			addrevs($1 - 1, $2);
		    } else {
			addrevs($1, $2 - 1);
		    }
		} else {
		    usage();
		}
	    }
	    shift;
	}
    }

    while (@ARGV) {
	if ($ARGV[0] eq 'from') {
	    shift;
	    if (@ARGV < 1) {
		usage();
	    }
	    $src_branch = $ARGV[0];
	    shift;
	} elsif ($ARGV[0] eq 'into') {
	    shift;
	    if (@ARGV < 1) {
		usage();
	    }
	    $tgt_dir = $ARGV[0];
	    shift;
	    if (!-d $tgt_dir) {
		usage();
	    }
	} else {
	    usage();
	}
    }

    examine();
    revs2ranges();
    printranges(\*STDERR)
	if $debug;
    fmerge();
}

1;
