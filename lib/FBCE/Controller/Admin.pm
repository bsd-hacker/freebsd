package FBCE::Controller::Admin;
use Moose;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Controller' }

=head1 NAME

FBCE::Controller::Admin - Catalyst Controller

=head1 DESCRIPTION

Catalyst Controller.

=head1 METHODS

=cut


=head2 index

=cut

sub index :Path :Args(0) {
    my ( $self, $c ) = @_;

    my $user = $c->user->get_object();
    if (!$user->admin) {
	$c->res->redirect($c->uri_for('/'));
	$c->detach();
    }
    my $voters = $c->model('FBCE::Person')->
	search(undef, { order_by => 'login' });
    my $candidates = $c->model('FBCE::Statement')->
	search_related('person', {}, { order_by => 'login' });
    my $voted = $c->model('FBCE::Vote')->
	search_related('voter', {}, { distinct => 1 });
    my $votes = $c->model('FBCE::Vote');
    my $results = $c->model('FBCE::Result')->
	search(undef, { order_by => { -desc => 'votes' } });
    $c->stash(voters => $voters);
    $c->stash(candidates => $candidates);
    $c->stash(voted => $voted);
    $c->stash(votes => $votes);
    $c->stash(results => $results);
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
