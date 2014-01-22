package Bugzilla::Extension::AutoAssigner;

use strict;
use base qw(Bugzilla::Extension);

use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Group;
use Bugzilla::User;
use Bugzilla::User::Setting;
use Bugzilla::Util qw(diff_arrays html_quote);
use Bugzilla::Status qw(is_open_state);
use Bugzilla::Install::Filesystem;

use constant REL_EXAMPLE => -127;

our $VERSION = '1.0';

use Data::Dumper;

sub bug_end_of_create {
    my ($self, $args) = @_;

    # This code doesn't actually *do* anything, it's just here to show you
    # how to use this hook.
    my $bug = $args->{'bug'};
    my $timestamp = $args->{'timestamp'};

    my $bug_id = $bug->id;
    my $subject = $bug->short_desc;
    my $component = $bug->component;
    if ($component eq 'ports') {
        if ($subject =~ /^\s*(\S+\/\S+)\s*:/) {
            my $port = $1;
            my $dbh = Bugzilla->dbh;
            my $sth = $dbh->prepare("SELECT maintainer FROM freebsd_ports_index where port=?");
            $sth->execute($port);
            my ($maintainer) = $sth->fetchrow_array();
            return unless(defined($maintainer));
            eval {
                my $user = Bugzilla::User->check($maintainer);
            };
            return if ($@ ne '');
            $bug->add_cc($maintainer);
            $bug->update();
        }
    }
}

sub db_schema_abstract_schema {
    my ($class, $args) = @_;
    my $schema = $args->{schema};

    $schema->{freebsd_ports_index} = {
        FIELDS => [
            port => {TYPE => 'varchar(255)', NOTNULL => 1},
            maintainer => {TYPE => 'varchar(255)', NOTNULL => 1},
        ],
        INDEXES => [
            freebsd_ports_index_port_id => { FIELDS => ['port'], TYPE => 'UNIQUE' },
        ],
    };
}

# This must be the last line of your extension.
__PACKAGE__->NAME;
