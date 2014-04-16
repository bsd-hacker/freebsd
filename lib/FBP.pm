use utf8;
package FBP;
use Moose;
use namespace::autoclean;

use Catalyst::Runtime 5.80;

# Set flags and add plugins for the application.

use Catalyst qw/
    ConfigLoader
    DateTime
    Authentication
    Authentication::Credential::Password
    Session
    Session::State::Cookie
    Session::Store::FastMmap
    Static::Simple
/;

extends 'Catalyst';

our $VERSION = '0.01';

# Configure the application.

__PACKAGE__->config(
    name => 'FBP',
    encoding => 'UTF-8',
    'Plugin::ConfigLoader' => {
        substitutions => {
            UID => sub { $< },
            PID => sub { $$ },
        },
    },
    'Plugin::Static::Simple' => {
        dirs => [ 'static' ],
    },
    'Plugin::Authentication' => {
        default_realm => 'fbp',
        fbp => {
            credential => {
                class => 'Password',
                password_field => 'password',
                password_type  => 'salted_hash',
            },
            store => {
                class => 'DBIx::Class',
                user_model => 'FBP::Person',
            },
        },
    },
    # Disable deprecated behavior needed by old applications
    disable_component_resolution_regex_fallback => 1,
);

sub now($) {
    my ($self) = @_;

    $self->stash->{now} //= DateTime->now();
}

# True for PostgreSQL if you have p5-DateTime-Format-Pg installed
$ENV{DBIC_DT_SEARCH_OK} = 1;

# Start the application
__PACKAGE__->setup();


=head1 NAME

FBP - Catalyst based application

=head1 SYNOPSIS

    script/fbp_server.pl

=head1 DESCRIPTION

FreeBSD Polls

=head1 SEE ALSO

L<FBP::Controller::Root>, L<Catalyst>

=head1 AUTHOR

Dag-Erling Smørgrav <des@freebsd.org>

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

1;

# $FreeBSD$
