package FBCE::Model::Rules;
use Moose;
use MooseX::Types::Common::Numeric qw(PositiveInt);
use MooseX::Types::DateTime::MoreCoercions qw(Duration);
use DateTime;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Component' }

=head1 NAME

FBCE::Controller - Catalyst Controller

=head1 DESCRIPTION

Catalyst Controller.

=cut

has max_votes => (
    isa => PositiveInt,
    is => 'ro',
    required => 1
);

has cutoff => (
    isa => Duration,
    coerce => 1,
    is => 'ro',
    required => 1,
);

our $cutoff_date;

sub cutoff_date($) {
    my ($self) = @_;

    if (!defined($cutoff_date)) {
	$cutoff_date =
	    FBCE->model('Schedule')->nominating_starts - $self->cutoff;
	$cutoff_date->set(hour => 0, minute => 0, second => 0);
	print STDERR $cutoff_date->ymd();
    }
    return $cutoff_date;
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
