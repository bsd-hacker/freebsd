package FBCE;
use Moose;
use MooseX::Types::Common::Numeric qw(PositiveInt);
use namespace::autoclean;

use Catalyst::Runtime 5.80;

use Catalyst qw/
    ConfigLoader
    Authentication
    Authentication::Credential::Password
    Session
    Session::State::Cookie
    Session::Store::FastMmap
    Static::Simple
    Unicode
/;

extends 'Catalyst';

our $VERSION = '0.01';
$VERSION = eval $VERSION;

# Configure the application.

__PACKAGE__->config(
    name => 'FBCE',
    view => 'HTML',
    'Plugin::Static::Simple' => {
	dirs => [ 'static' ],
    },
    'Plugin::Authentication' => {
	default_realm => 'fbce',
	fbce => {
	    credential => {
		class => 'Password',
		password_field => 'password',
		password_type  => 'salted_hash',
	    },
	    store => {
		class => 'DBIx::Class',
		user_model => 'FBCE::Person',
	    },
	},
    },
    # Disable deprecated behavior needed by old applications
    disable_component_resolution_regex_fallback => 1,
);

# Start the application
__PACKAGE__->setup();

=head1 NAME

FBCE - Catalyst based application

=head1 SYNOPSIS

    script/fbce_server.pl

=head1 DESCRIPTION

[enter your description here]

=head1 SEE ALSO

L<FBCE::Controller::Root>, L<Catalyst>

=head1 AUTHOR

Dag-Erling Smørgrav

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

1;

# $FreeBSD$
