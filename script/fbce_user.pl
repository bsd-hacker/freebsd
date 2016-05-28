#!/usr/bin/env perl

use Catalyst::ScriptRunner;
Catalyst::ScriptRunner->run('FBCE', 'User');

1;

# $FreeBSD$

=encoding utf8

=head1 NAME

fbce_user.pl - Manage FBCE Users

=head1 SYNOPSIS

fbce_user.pl [options] command [arguments]

 Options:
   --debug        print additional information while working
   --dryrun       don't actually do anything
   --pwfile       name of password file for pwgen command
   --pwtarball    name of password tarball for pwgen command
   --help         show this message and exit

 Commands:

   list           list existing users
   import         import new users
   gecos          set real name for listed user(s)
   pwgen          generate passwords
   smash          clear active and incumbent bit for all users
   activate	  set active bit for listed user(s)
   deactivate	  clear active bit for listed user(s)
   incumbent      set incumbent bit for listed user(s)

=head1 DESCRIPTION

The B<fbce_user> script is used to manage users in the FBCE system.
The following commands are available:

=over

=item B<list>

List all users.  Prints one line per user with their login, active
status, admin status and name as recorded in the database.

=item B<import> [I<file> ...]

Import users.  If the input includes the users' names, those will be
imported too; otherwise, their names will be set equal to their login
names.

=item B<gecos> [I<file> ...]

Set the specified users' names to those indicated in the input.  Note
that if the input includes lines where no name is specified, those
users' names will be reset to their login.

=item B<pwgen>

Generate passwords for users that don't already have one.  This
command will also generate a tarball containing individual files for
each user, containing that user's password, in a subdirectory bearing
the user's name.  Each file's owner and group will be set equal to the
corresponding user's login.

=item B<smash>

Clear all users' active and incumbent bits.

=item B<activate> [I<file> ...]

Mark the specified users as active, allowing them to run and vote in
the election.

=item B<deactivate> [I<file> ...]

Mark the specified users as inactive, preventing them from running or
voting in the election.

=item B<incumbent> [I<file> ...]

Mark the specified users as incumbents so they are listed as such in
the list of candidates presented to voters.

=back

=head2 Input Format

All commands that operate on a list of users, rather than on the
entire user base, expect that list to be provided either on stdin or
in files listed on the command line.  Each line in the input must be
in one of the following formats:

=over

=item *

login only (any leading or trailing whitespace is ignored)

=item *

login and real name separated by whitespace (any leading or trailing
whitespace is ignored)

=item *

Unix v7 (seven-field) passwd format

=item *

BSD (ten-field) passwd format

=back

Any input which B<fbce_user> doesn't understand will simply be
ignored.

=head1 AUTHORS

Dag-Erling Smørgrav <des@FreeBSD.org>

=head1 COPYRIGHT

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
