use utf8;
use open ':locale';
package FBCE::Script::User;

use Moose;
use MooseX::Types::Moose qw/Bool Str/;
use FBCE;
use Archive::Tar;
use namespace::autoclean;

with 'Catalyst::ScriptRole';

has debug => (
    traits        => [qw(Getopt)],
    cmd_aliases   => 'd',
    isa           => Bool,
    is            => 'ro',
    documentation => q{Debugging mode},
);

has dryrun => (
    traits        => [qw(Getopt)],
    cmd_aliases   => 'n',
    isa           => Bool,
    is            => 'ro',
    documentation => q{Dry run},
);

has pwtarball => (
    traits	  => [qw(Getopt)],
    cmd_aliases	  => 't',
    isa		  => Str,
    is            => 'ro',
    documentation => q{Name of password tarball},
    default       => 'fbce-passwords.tgz',
);

has pwfile => (
    traits	  => [qw(Getopt)],
    cmd_aliases	  => 'f',
    isa		  => Str,
    is            => 'ro',
    documentation => q{Name of password file},
    default       => 'fbce-password',
);

#
# Read a list of users.
#
sub _read_users($@) {
    my ($self, @argv) = @_;

    my %users;
    @ARGV = @argv;
    while (<>) {
	chomp();
	if (m/^\s*(\w+)\s*$/) {
	    # login
	    $users{$1} = $1;
	} elsif (m/^\s*(\w+)\s+(\S.*\S)\s*$/) {
	    # login gecos
	    $users{$1} = $2;
	} elsif (m/^(\w+)(?::[^:]*){3}:([^:,]*)(?:,[^:]*)?(?::[^:]*){2}$/) {
	    # v7 passwd file
	    $users{$1} = $2 || $1;
	} elsif (m/^(\w+)(?::[^:]*){6}:([^:,]*)(?:,[^:]*)?(?::[^:]*){2}$/) {
	    # BSD passwd file
	    $users{$1} = $2 || $1;
	} else {
	    # ignore
	}
    }
    return \%users;
}

#
# Activate or deactivate named users
#
sub _set_active($$@) {
    my ($self, $active, @users) = @_;

    my $persons = FBCE->model('FBCE::Person');
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	foreach my $login (@users) {
	    my $person = $persons->find({ login => $login });
	    if ($person) {
		warn("marking $login " .
		     ($active ? "active" : "inactive") . "\n")
		    if $self->debug;
		$person->update({ active => $active });
	    } else {
		warn("No such user: $login\n");
	    }		
	}
	$schema->txn_rollback()
	    if $self->dryrun;
    });
}

#
# Mark named users as incumbent or not
#
sub _set_incumbent($$@) {
    my ($self, $incumbent, @users) = @_;

    my $persons = FBCE->model('FBCE::Person');
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	foreach my $login (@users) {
	    my $person = $persons->find({ login => $login });
	    if ($person) {
		warn("marking $login " .
		     ($incumbent ? "incumbent" : "inincumbent") . "\n")
		    if $self->debug;
		$person->update({ incumbent => $incumbent });
	    } else {
		warn("No such user: $login\n");
	    }		
	}
	$schema->txn_rollback()
	    if $self->dryrun;
    });
}

#
# List existing users
#
sub cmd_list($@) {
    my ($self, @argv) = @_;

    die("too many arguments")
	if @argv;
    my $persons = FBCE->model('FBCE::Person')->
	search(undef, { order_by => 'login' });
    printf("%-16s%-8s%-8s%-8s%s\n",
	   'login',
	   'inc',
	   'act',
	   'adm',
	   'name');
    foreach my $person ($persons->all()) {
	printf("%-16s%-8s%-8s%-8s%s\n",
	       $person->login,
	       $person->incumbent ? 'yes' : 'no',
	       $person->active ? 'yes' : 'no',
	       $person->admin ? 'yes' : 'no',
	       $person->name);
    }
}

#
# Mark all users inactive
#
sub cmd_smash($@) {
    my ($self, @argv) = @_;

    die("too many arguments")
	if @argv;
    my $persons = FBCE->model('FBCE::Person');
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	foreach my $person ($persons->all) {
	    $person->update({ active => 0, incumbent => 0 });
	}
	$schema->txn_rollback()
	    if $self->dryrun;
    });
}

#
# Activate named users
#
sub cmd_activate(@) {
    my ($self, @argv) = @_;

    my $users = $self->_read_users(@argv);
    $self->_set_active(1, keys %$users);
}

#
# Deactivate named users
#
sub cmd_deactivate(@) {
    my ($self, @argv) = @_;

    my $users = $self->_read_users(@argv);
    $self->_set_active(0, keys %$users);
}

#
# Mark the specified user(s) as incumbent
#
sub cmd_incumbent(@) {
    my ($self, @argv) = @_;

    my $users = $self->_read_users(@argv);
    $self->_set_incumbent(1, keys %$users);
}

#
# Read a list of users from a file and create corresponding database
# records.  This will not touch existing users.
#
sub cmd_import(@) {
    my ($self, @argv) = @_;

    my $users = $self->_read_users(@argv);
    my $persons = FBCE->model('FBCE::Person');
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	while (my ($login, $gecos) = each(%$users)) {
	    my $person = $persons->find_or_new({ login => $login });
	    next if $person->in_storage;
	    warn("importing user $login\n")
		if $self->debug;
	    $person->set_columns({ realname => $gecos });
	    $person->update_or_insert();
	}
	$schema->txn_rollback()
	    if $self->dryrun;
    });
}

#
# Read a list of users from a file and set their names accordingly.
# Users that are listed in the file but not in the database will be
# ignored.
#
sub cmd_gecos($@) {
    my ($self, @argv) = @_;

    my $users = $self->_read_users(@argv);
    my $persons = FBCE->model('FBCE::Person');
    my $schema = $persons->result_source()->schema();
    $schema->txn_do(sub {
	while (my ($login, $gecos) = each(%$users)) {
	    my $person = $persons->find({ login => $login })
		or next;
	    $person->update({ realname => $gecos });
	}
	$schema->txn_rollback()
	    if $self->dryrun;
    });
}

#
# Use sysutils/pwgen to generate random passwords
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
    die(sprintf("expected %d passwords, got %d\n", $n, int(@passwords)))
	unless @passwords == $n;
    warn("got $n passwords as expected\n")
	if $self->debug;
    return @passwords;
}

#
# Generate passwords users that don't already have one.  Use with
# caution!
#
sub cmd_pwgen($@) {
    my ($self, @argv) = @_;

    die("too many arguments")
	if @argv;

    # Please don't overwrite an existing password tarball!
    my $tarball = $self->pwtarball;
    die("$tarball exists, delete or move and try again\n")
	if -e $tarball;
    my $pwfile = $self->pwfile;

    # Generate enough passwords for everybody
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
	foreach my $person ($persons->all) {
	    my ($login, $password) = ($person->login, shift(@passwords));
	    warn("setting password for $login\n")
		if $self->debug;
	    $person->set_password($password);
	    $tar->add_data("$login/$pwfile", "$password\n",
			   { uname => $login, gname => $login, mode => 0400 });
	}
	warn("writing the tar file\n")
	    if $self->debug;
	$tar->write($tarball, COMPRESS_GZIP)
	    or die($tar->error());
	$schema->txn_rollback()
	    if $self->dryrun;
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
    } elsif ($command eq 'import') {
	$self->cmd_import(@{$self->extra_argv});
    } elsif ($command eq 'smash') {
	$self->cmd_smash(@{$self->extra_argv});
    } elsif ($command eq 'activate') {
	$self->cmd_activate(@{$self->extra_argv});
    } elsif ($command eq 'deactivate') {
	$self->cmd_deactivate(@{$self->extra_argv});
    } elsif ($command eq 'incumbent') {
	$self->cmd_incumbent(@{$self->extra_argv});
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

=head1 AUTHOR

Dag-Erling Smørgrav

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
