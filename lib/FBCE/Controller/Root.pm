package FBCE::Controller::Root;
use Moose;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Controller' }

#
# Sets the actions in this controller to be registered with no prefix
# so they function identically to actions created in MyApp.pm
#
__PACKAGE__->config(namespace => '');

=head1 NAME

FBCE::Controller::Root - Root Controller for FBCE

=head1 DESCRIPTION

[enter your description here]

=head1 METHODS

=head2 index

The root page (/)

=cut

sub auto :Private {
    my ($self, $c) = @_;

    # Stash schedule information etc.
    $c->stash(title => FBCE->config->{'title'});
    my $now = DateTime->now();
    $c->stash(now => $now);
    my $schedule = $c->comp('FBCE::Model::Schedule');
    foreach my $phase ("nominating", "voting") {
	foreach my $endpoint ("${phase}_starts", "${phase}_ends") {
	    $c->stash($endpoint => $schedule->{$endpoint});
	}
    }
    $c->stash(announcement => $schedule->{'announcement'});
    $c->stash(investiture => $schedule->{'investiture'});
    $c->stash(nominating => $schedule->nominating($now));
    $c->stash(voting => $schedule->voting($now));
    $c->stash(announced => $schedule->announced($now));
    # XXX does not really belong in FBCE::Schedule
    $c->stash(max_votes => $schedule->{'max_votes'});

    # Authentication
    if ($c->request->path !~ m/^(login|logout|bylaws|mission|static\/.*)?$/) {
	if (!$c->user_exists) {
	    $c->stash(action => $c->uri_for());
	    $c->stash(template => 'login.tt');
	    return 0;
	}
    }
    if ($c->user) {
	$c->stash(user => $c->user->get_object());
    }

    return 1;
}

sub login :Local :Args(0) {
    my ($self, $c) = @_;

    my ($login, $password, $action) =
	@{$c->request->params}{'login', 'password', 'action'};
    if ($login && $password) {
	$c->authenticate({
	    login => $c->request->params->{'login'},
	    password => $c->request->params->{'password'}
	});
    }
    if ($c->user_exists) {
	if ($action) {
	    $c->response->redirect($action);
	} else {
	    $c->response->redirect($c->uri_for('/'));
	}
	return;
    }
    $c->stash(action => $action);
}

sub logout :Local :Args(0) {
    my ($self, $c) = @_;

    $c->logout();
    $c->response->redirect($c->uri_for('/'));
}

sub index :Path :Args(0) {
    my ($self, $c) = @_;

}

sub bylaws :Local :Args(0) {
    my ($self, $c) = @_;

}

sub mission :Local :Args(0) {
    my ($self, $c) = @_;

}

=head2 default

Standard 404 error page

=cut

sub default :Path {
    my ($self, $c) = @_;

    $c->response->body('Page not found');
    $c->response->status(404);
}

=head2 end

Attempt to render a view, if needed.

=cut

sub end : ActionClass('RenderView') {}

=head1 AUTHOR

Dag-Erling Smørgrav

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

__PACKAGE__->meta->make_immutable;

1;

# $FreeBSD$
