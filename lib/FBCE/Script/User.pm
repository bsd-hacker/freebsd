package FBCE::Script::User;

use Moose;
use MooseX::Types::Common::Numeric qw/PositiveInt/;
use MooseX::Types::Moose qw/Str Bool Int/;
use FBCE;
use Archive::Tar;
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

# Survey URLs for various repos
sub survey_url($) { "http://people.freebsd.org/~peter/$_[0].total.txt" }

# Name of password tarball
our $pwtar = 'fbce-passwords.tgz';

#
# Download and parse Peter Wemm's survey for a specific repo
#
sub retrieve_commit_data($$) {
    my ($self, $repo) = @_;

    # create new user agent unless one already exists
    $self->{user_agent} //= LWP::UserAgent->new(%lwp_options);
    my $url = survey_url($repo);
    my $req = HTTP::Request->new(GET => $url);
    warn("Retrieving $url...\n")
	if $self->debug;
    my $res = $self->{user_agent}->request($req);
    if (!$res->is_success()) {
	die("$url: " . $res->status_line() . "\n");
    }
    my $survey = $res->decoded_content();
    foreach (split('\n', $survey)) {
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
                       (?:\w+)\s+
                       (?:\d+)\s+
                       (?:\d+)\s+
                       (\w+)\s*$/x &&
		       $1 > 0 && $2 > 0 && $3 > 0;
	my $date = DateTime->new(year => $1, month => $2, day => $3,
				 time_zone => 'UTC');
	my $login = $4;
	if (defined($self->{committers}->{$login}) &&
	    DateTime->compare($date, $self->{committers}->{$login}) < 0) {
#	    warn(sprintf("skipping %s: %s < %s\n", $login, $date->ymd(),
#			 $self->{committers}->{$login}->ymd()))
#		if $self->debug;
	} else {
#	    warn(sprintf("adding %s: %s (%s)\n", $login, $date->ymd(), $repo))
#		if $self->debug;
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
	       $person->login,
	       $person->active ? 'yes' : 'no',
	       $person->admin ? 'yes' : 'no',
	       $person->name);
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
	$persons->reset();
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
	$self->retrieve_commit_data($repo);
    }

    # insert it into the database
    my $persons = FBCE->model('FBCE::Person');
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	while (my ($login, $last_commit) = each(%{$self->{committers}})) {
	    my $person = $persons->find_or_new({ login => $login });
	    my $active =
		DateTime->compare($last_commit, $cutoff_date) >= 0 ? 1 : 0;
	    if ($person->in_storage()) {
		if ($active != $person->active) {
		    warn(sprintf("updating %s: %s -> %s\n",
				 $person->login,
				 $person->active ? 'active' : 'inactive',
				 $active ? 'active' : 'inactive'))
			if $self->debug;
		    $person->update({ active => $active });
		}
	    } else {
		$person->set_column(active => $active);
		$person->insert();
	    }
	}
    });
}

#
# Set each user's realname column based on their gecos
#
sub cmd_gecos(@) {
    my ($self, $pwfn) = @_;

    my %gecos;

    # read passwd file
    $pwfn //= "/etc/passwd";
    open(my $pwfh, '<', $pwfn)
	or die("$pwfn: $!\n");
    warn("reading names from $pwfn\n")
	if $self->debug;
    while (<$pwfh>) {
	chomp($_);
	my @pwent = split(':', $_);
	next unless @pwent == 7;
	next unless $pwent[4] =~ m/^([^,]+)/;
	$gecos{$pwent[0]} = $1;
    }
    close($pwfh);

    # update the database
    my $persons = FBCE->model('FBCE::Person')->
	search({}, { order_by => 'login' });
    my $schema = $persons->result_source()->schema();
    my $n;
    $schema->txn_do(sub {
	warn("setting names in the database\n")
	    if $self->debug;
	$n = 0;
	$persons->reset();
	while (my $person = $persons->next) {
	    my $login = $person->login;
	    my $gecos = $gecos{$login};
	    next unless $gecos;
	    next if $person->realname;
	    $person->update({ realname => $gecos });
	    ++$n;
	}
	warn("$n record(s) updated\n")
	    if $self->debug;
    });
}

#
# Use sysutils/pwgen2 to generate random passwords
#
sub pwgen($$;$) {
    my ($self, $n, $len) = @_;

    $len ||= 12;
    warn("generating $n $len-character passwords\n")
	if $self->debug;

    # Set up a pipe and fork a child
    my $pid = open(my $pipe, '-|');
    if (!defined($pid)) {
	# fork failed
	die("fork(): $!\n");
    } elsif ($pid == 0) {
	# child process - run pwgen
	# ugh hardcoded...
        exec('/usr/local/bin/pwgen', '-can', $len, $n);
        die("child: exec(): $!\n");
    }

    # read output from child
    my @passwords;
    while (<$pipe>) {
	m/^([0-9A-Za-z]{$len})$/
	    or die("invalid output from pwgen\n");
	push(@passwords, $1);
    }

    # check exit status
    if (waitpid($pid, 0) != $pid) {
        if ($? & 0xff) {
            die(sprintf("pwgen caught signal %d\n", $? & 0x7f));
        } elsif ($? >> 8) {
            die(sprintf("pwgen exited with code %d\n", $? >> 8));
        } else {
            die("waitpid(): $!\n");
        }
    }
    close($pipe);

    # sanity check and we're done
    die(sprintf("expected %d passwords, got %d\n", $n, @passwords))
	unless @passwords == $n;
    warn("got $n passwords as expected\n")
	if $self->debug;
    return @passwords;
}

#
# Generate passwords for all users.  Use with caution!
#
sub cmd_pwgen(@) {
    my ($self, @users) = @_;

    # please don't overwrite an existing password tarball...
    die("$pwtar exists, delete or move and try again\n")
	if -e $pwtar;

    # generate enough passwords for everybody
    my $persons = FBCE->model('FBCE::Person')->
	search({ password => '*' }, { order_by => 'login' });
    my $n = $persons->count();
    my @passwords = $self->pwgen($n);

    # create the archive
    my $tar = Archive::Tar->new();

    # update the database and the archive
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	warn("setting the passwords in the database\n")
	    if $self->debug;
	$persons->reset();
	while (my $person = $persons->next) {
	    my ($login, $password) = ($person->login, shift(@passwords));
	    # printf("%s\t%s\n", $person->login, $password);
	    warn("setting password for $login\n")
		if $self->debug;
	    $person->set_password($password);
	    $tar->add_data("$login/election-password", "$password\n",
			   { uname => $login, gname => $login, mode => 0400 });
	}
	warn("writing the tar file\n")
	    if $self->debug;
	$tar->write($pwtar, COMPRESS_GZIP)
	    or die($tar->error());
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
    } elsif ($command eq 'gecos') {
	$self->cmd_gecos(@{$self->extra_argv});
    } elsif ($command eq 'pwgen') {
	$self->cmd_pwgen(@{$self->extra_argv});
    } else {
	die("unrecognized command.\n");
    }
}

__PACKAGE__->meta->make_immutable;

1;

# $FreeBSD$
