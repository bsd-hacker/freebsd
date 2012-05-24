package FBCE::Model::Schedule;
use Moose;
use MooseX::Types::Common::Numeric qw(PositiveInt);
use MooseX::Types::DateTime::MoreCoercions qw(DateTime Duration);
use DateTime;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Component' }

=head1 NAME

FBCE::Controller - Catalyst Controller

=head1 DESCRIPTION

Catalyst Controller.

=cut

has nominating_starts => (
    isa => DateTime,
    coerce => 1,
    is => 'ro',
    required => 1
);

has nominating_ends => (
    isa => DateTime,
    coerce => 1,
    is => 'ro',
    required => 1
);

has voting_starts => (
    isa => DateTime,
    coerce => 1,
    is => 'ro',
    required => 1
);

has voting_ends => (
    isa => DateTime,
    coerce => 1,
    is => 'ro',
    required => 1
);

has announcement => (
    isa => DateTime,
    coerce => 1,
    is => 'ro',
    required => 1
);

has investiture => (
    isa => DateTime,
    coerce => 1,
    is => 'ro',
    required => 1
);

sub _phase($$$) {
    my ($self, $phase, $now) = @_;

    $now //= main::DateTime->now();
    my ($starts, $ends) = ("${phase}_starts", "${phase}_ends");
    my ($st, $et) = ($self->{$starts}, $self->{$ends});
    if (main::DateTime->compare($now, $st) < 0) {
	return -1;
    } elsif (main::DateTime->compare($now, $et) < 0) {
	return 0;
    } else {
	return 1;
    }
}

sub nominating($;$) {
    my ($self, $now) = @_;

    return $self->_phase('nominating', $now);
}

sub voting($;$) {
    my ($self, $now) = @_;

    return $self->_phase('voting', $now);
}

sub announced($;$) {
    my ($self, $now) = @_;

    return (main::DateTime->compare($now, $self->{'announcement'}) > 0);
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
