package FBCE::Controller;
use Moose;
use DateTime;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Controller' }

=head1 NAME

FBCE::Controller - Catalyst Controller

=head1 DESCRIPTION

Catalyst Controller.

=head1 METHODS

=cut

=head2 auto

=cut

sub auto :Private {
    my ($self, $c) = @_;

    $c->stash(title => FBCE->config->{'title'});
    my $now = DateTime->now();
    $c->stash(now => $now);
    my $schedule = $c->comp('FBCE::Model::Schedule');
    foreach my $phase ("nominating", "voting") {
       foreach my $endpoint ("${phase}_starts", "${phase}_ends") {
	   $c->stash($endpoint => $schedule->{$endpoint});
       }
    }
    $c->stash('announcement' => $schedule->{'announcement'});
    $c->stash('investiture' => $schedule->{'investiture'});
    $c->stash(nominating => $schedule->nominating($now));
    $c->stash(voting => $schedule->voting($now));
    # XXX does not really belong in FBCE::Schedule
    $c->stash(max_votes => $schedule->{'max_votes'});
}

=head1 AUTHOR

Dag-Erling Smørgrav

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

__PACKAGE__->meta->make_immutable;

1;
