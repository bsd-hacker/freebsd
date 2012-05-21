package FBCE::Controller::Run;
use Moose;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Controller' }

=head1 NAME

FBCE::Controller::Run - Catalyst Controller

=head1 DESCRIPTION

Catalyst Controller.

=head1 METHODS

=cut

=head2 index

=cut

sub index :Path :Args(0) {
    my ($self, $c) = @_;

#    $c->authenticate();
    my $user = $c->user->get_object();
    $c->stash(user => $user);
}

sub register :Local :Args(0) {
    my ($self, $c) = @_;

#    $c->authenticate();
    my $user = $c->user->get_object();
    if ($c->stash->{'nominating'} != 0) {
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    $c->stash(user => $user);
    if ($user->statement) {
	$c->res->redirect($c->uri_for('/run/edit'));
	$c->detach();
    }
    my $p = $c->req->params;
    if ($p->{cancel}) {
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    if ($p->{submit}) {
	my $stmt = $c->model('FBCE::Statement')->
	    new({ person => $user, short => $p->{short}, long => $p->{long} });
	$stmt->insert()
	    or die("failed to register");
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    $c->stash(short => $p->{short});
    $c->stash(long => $p->{long});
}

sub edit :Local :Args(0) {
    my ($self, $c) = @_;

#    $c->authenticate();
    my $user = $c->user->get_object();
    if ($c->stash->{'nominating'} != 0) {
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    $c->stash(user => $user);
    if (!$user->statement) {
	$c->res->redirect($c->uri_for('/run/register'));
	$c->detach();
    }
    my $p = $c->req->params;
    if ($p->{cancel}) {
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    my $statement = $user->statement;
    if ($p->{submit}) {
	$statement->update({ short => $p->{short}, long => $p->{long} })
	    or die("failed to update");
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    $c->stash(short => $p->{short} // $statement->short);
    $c->stash(long => $p->{long} // $statement->long);
}

sub withdraw :Local :Args(0) {
    my ($self, $c) = @_;

#    $c->authenticate();
    my $user = $c->user->get_object();
    if ($c->stash->{'nominating'} != 0) {
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    $c->stash(user => $user);
    if (!$user->statement) {
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    my $p = $c->req->params;
    if ($p->{cancel}) {
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
    my $statement = $user->statement;
    if ($p->{submit}) {
	$statement->delete()
	    or die("failed to delete");
	$c->res->redirect($c->uri_for('/run'));
	$c->detach();
    }
}

=head1 AUTHOR

Dag-Erling Smørgrav

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

__PACKAGE__->meta->make_immutable;

1;
