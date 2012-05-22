package FBCE::Script::User;

use Moose;
use MooseX::Types::Common::Numeric qw/PositiveInt/;
use MooseX::Types::Moose qw/Str Bool Int/;
use FBCE;
use LWP::UserAgent;
use namespace::autoclean;

use Data::Dumper;

with 'Catalyst::ScriptRole';

has debug => (
    traits        => [qw(Getopt)],
    cmd_aliases   => 'd',
    isa           => Bool,
    is            => 'ro',
    documentation => q{Debugging mode},
);

# XXX should be traits
our %lwp_options = (
    timeout => 10,
    env_proxy => 1,
    keep_alive => 1,
);

# Cutoff URLs for various repos
sub cutoff_url($) { "http://people.freebsd.org/~peter/$_[0].cutoff.txt" }

#
# Download and parse Peter Wemm's cutoff list for a specific repo
#
sub retrieve_cutoff_data($$) {
    my ($self, $repo) = @_;

    # create new user agent unless one already exists
    $self->{user_agent} //= LWP::UserAgent->new(%lwp_options);
    my $url = cutoff_url($repo);
    my $req = HTTP::Request->new(GET => $url);
    warn("Retrieving $url...\n")
	if $self->debug;
    my $res = $self->{user_agent}->request($req);
    if (!$res->is_success()) {
	die("$url: " . $res->status_line() . "\n");
    }
    my $cutoff = $res->decoded_content();
    foreach (split('\n', $cutoff)) {
	#
	# Each line looks like this:
	#
	# 20120430 ok         84    95 des
	#
	# The first column is the date of the last commit.  The second
	# column is "ok" if this committer has a commit bit in this
	# repo, "visitor" if they have a commit bit in a different
	# repo or "-" if they are retired.  The third and fourth
	# columns are not relevant to us.  The fifth is the freefall
	# login.
	#
	next unless m/^(\d\d\d\d)(\d\d)(\d\d)\s+
                       (?:ok|visitor)\s+
                       (?:\d+)\s+
                       (?:\d+)\s+
                       (\w+)\s*$/x &&
		       $1 > 0 && $2 > 0 && $3 > 0;
	my $date = DateTime->new(year => $1, month => $2, day => $3,
				 time_zone => 'UTC');
	my $login = $4;
	if (defined($self->{committers}->{$login}) &&
	    DateTime->compare($date, $self->{committers}->{$login}) < 0) {
	    warn(sprintf("skipping %s: %s < %s\n", $login, $date->ymd(),
			 $self->{committers}->{$login}->ymd()))
		if $self->debug;
	} else {
	    warn(sprintf("adding %s: %s (%s)\n", $login, $date->ymd(), $repo))
		if $self->debug;
	    $self->{committers}->{$login} = $date;
	}
    }
}

#
# List existing users
#
sub cmd_list(@) {
    my ($self) = @_;

    die("too many arguments")
	if @{$self->ARGV};
    my $persons = FBCE->model('FBCE::Person')->
	search({}, { order_by => 'login' });
    printf("%-16s%-8s%-8s%s\n",
	   'login',
	   'active',
	   'admin',
	   'name');
    foreach my $person ($persons->all()) {
	printf("%-16s%-8s%-8s%s\n",
	       $person->login(),
	       $person->active() ? 'yes' : 'no',
	       $person->admin() ? 'yes' : 'no',
	       $person->name());
    }
}

#
# Mark all users inactive
#
sub cmd_smash(@) {
    my ($self) = @_;

    my $persons = FBCE->model('FBCE::Person')->search();
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	while (my $person = $persons->next) {
	    $person->update({ active => 0 });
	}
    });
}

#
# Pull the list of active committers; create users for committers that
# don't already have one, and set the active bit.
#
sub cmd_pull(@) {
    my ($self) = @_;

    # cutoff duration from config
    my $cutoff_duration = FBCE->model('Schedule')->cutoff;

    # cutoff date: start out with current time (UTC)
    my $cutoff_date = DateTime->now(time_zone => 'UTC');
    # round down to midnight
    $cutoff_date->set(hour => 0, minute => 0, second => 0);
    # subtract the cutoff duration
    $cutoff_date->subtract_duration($cutoff_duration);
    warn(sprintf("Setting cutoff date to %sT%sZ\n",
		 $cutoff_date->ymd(), $cutoff_date->hms()))
	if $self->debug;

    # pull "last commit" data for src, ports and doc / www repos
    foreach my $repo (qw(src ports docwww)) {
	$self->retrieve_cutoff_data($repo);
    }

    # insert it into the database
    my $persons = FBCE->model('FBCE::Person');
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	while (my ($login, $last_commit) = each(%{$self->{committers}})) {
	    my $person = $persons->find_or_new({ login => $login });
	    my $active =
		DateTime->compare($last_commit, $cutoff_date) >= 0 ? 1 : 0;
	    warn(sprintf("%s %s (%s)\n",
			 $person->in_storage() ? 'updating' : 'inserting',
			 $person->login(),
			 $active ? 'active' : 'inactive'))
		if $self->debug;
	    $person->set_column(active => $active);
	    $person->update_or_insert();
	}
    });
}

sub run($) {
    my ($self) = @_;

    local $ENV{CATALYST_DEBUG} = 1
        if $self->debug;

    my $command = shift(@{$self->extra_argv})
	or die("command required\n");
    if ($command eq 'list') {
	$self->cmd_list(@{$self->extra_argv});
    } elsif ($command eq 'smash') {
	$self->cmd_smash(@{$self->extra_argv});
    } elsif ($command eq 'pull') {
	$self->cmd_pull(@{$self->extra_argv});
    } else {
	die("unrecognized command.\n");
    }
}

__PACKAGE__->meta->make_immutable;

1;

# $FreeBSD$
