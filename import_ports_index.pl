#!/usr/local/bin/perl -w

use strict;
use warnings;

use lib qw(/usr/local/www/bugs42.freebsd.org/lib/ /usr/local/www/bugs42.freebsd.org/);
use Bugzilla;

open F, "< /var/ports/INDEX-9";

my %maintainers;

while(<F>) {
    chomp;
    my @fields = split /\|/;
    my $port = $fields[1];
    $port =~ s@/usr/ports/@@;
    my $maintainer = $fields[5];
    $maintainers{$port} = $maintainer;
}

my $dbh = Bugzilla->dbh;
$dbh->bz_start_transaction();
$dbh->do("DELETE from freebsd_ports_index");
my $sth = $dbh->prepare("insert into freebsd_ports_index values (?, ?)");
foreach my $k (keys %maintainers) {
    $sth->execute($k, $maintainers{$k});
}
$dbh->bz_commit_transaction();
