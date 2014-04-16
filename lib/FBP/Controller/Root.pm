package FBP::Controller::Root;
use utf8;
use Moose;
use namespace::autoclean;

BEGIN { extends 'FBP::Controller' }

#
# Sets the actions in this controller to be registered with no prefix
# so they function identically to actions created in MyApp.pm
#
__PACKAGE__->config(namespace => '');

=head1 NAME

FBP::Controller::Root - Root Controller for FBP

=head1 DESCRIPTION

[enter your description here]

=head1 METHODS

=head2 auto

Common code for every action

=cut

sub auto :Private {
    my ($self, $c) = @_;

    # Stash various constants
    $c->stash(title => $c->config->{'title'});

    # Stash active polls
    if ($c->user_exists) {
	my $polls = $c->model('FBP::Poll')->
	    search({ starts => { '<=', $c->now }, ends => { '>=', $c->now } });
	$c->stash(polls => $polls);
    }

    1;
}

=head2 index

The front page

=cut

sub index :Path :Args(0) {
    my ($self, $c) = @_;

    # nothing
}

=head2 login

Display the login page and process login information

=cut

sub login :Local :Args(0) {
    my ($self, $c) = @_;

    if ($c->user_exists) {
	my $login = $c->user->login;
	$c->response->redirect($c->uri_for('/polls'));
	$c->detach();
    }
    my ($login, $password) = @{$c->request->params}{'login', 'password'};
    if ($login && $password &&
	$c->authenticate({ login => $login, password => $password })) {
	$c->change_session_id();
	$c->response->redirect($c->uri_for('/polls'));
    }
}

=head2 logout

Log the user out and return to the front page

=cut

sub logout :Local :Args(0) {
    my ($self, $c) = @_;

    if ($c->user_exists) {
	my $login = $c->user->login;
	$c->delete_session();
	$c->logout();
    }
    $c->response->redirect($c->uri_for('/'));
}

=head2 polls

List of active polls.

=cut

sub polls :Local :Args(0) {
    my ($self, $c) = @_;

    $c->stash(title => 'Active polls');
}

=head2 help

Display help text.

=cut

sub help :Local :Args(0) {
    my ($self, $c) = @_;

    $c->stash(title => 'Help');
}

=head2 default

Default page.

=cut

sub default :Path {
    my ($self, $c) = @_;

    $c->stash(template => 'fof.tt');
    $c->response->status(404);
}

=head2 end

Attempt to render a view, if needed.

=cut

sub end : ActionClass('RenderView') {}

=head1 AUTHOR

Dag-Erling Smørgrav <des@freebsd.org>

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

__PACKAGE__->meta->make_immutable;

1;

# $FreeBSD$
