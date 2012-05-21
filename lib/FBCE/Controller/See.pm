package FBCE::Controller::See;
use Moose;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Controller' }

=head1 NAME

FBCE::Controller::See - Catalyst Controller

=head1 DESCRIPTION

Catalyst Controller.

=head1 METHODS

=cut


=head2 index

=cut

sub index :Path :Args(0) {
    my ($self, $c) = @_;

    my $user = $c->user->get_object();
    $c->stash(user => $user);
    my $candidates = $c->model('FBCE::Statement')->
	search_related('person', {}, { order_by => 'login' });
    $c->stash(candidates => $candidates);
}

sub candidate :Local :Args(1) {
    my ($self, $c, $name) = @_;

    my $user = $c->user->get_object();
    $c->stash(user => $user);
    my $candidate = $c->model('FBCE::Person')->find({ login => $name });
    if (!$candidate || !$candidate->statement) {
	$c->res->redirect($c->uri_for('/see'));
	$c->detach();
    }
    $c->stash(candidate => $candidate);
}

=head1 AUTHOR

Dag-Erling Smørgrav

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

__PACKAGE__->meta->make_immutable;

1;

# $FreeBSD$
