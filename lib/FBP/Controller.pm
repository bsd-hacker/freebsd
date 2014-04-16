use utf8;
package FBP::Controller;
use Moose;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Controller'; }

=encoding utf8

=head1 NAME

FBP::Controller - Superclass for FBP controllers

=head1 DESCRIPTION

This class provides common code for FBP controllers.

=head1 METHODS

=head2 require_user

Verifies that the client is authenticated, and if not, redirects to
the login page.

=cut

sub require_user($$) {
    my ($self, $c) = @_;

    if (!$c->user_exists) {
	$c->response->redirect($c->uri_for_action('/login'));
	$c->detach();
    }
    return $c->user;
}

=head1 AUTHOR

Dag-Erling Smørgrav <des@freebsd.org>

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

__PACKAGE__->meta->make_immutable;

1;

# $FreeBSD$
