use utf8;
package Template::Plugin::WikiFormat;

=head1 NAME

Template::Plugin::WikiFormat

=cut

use strict;
use warnings;
use base 'Template::Plugin::Filter';
use Text::WikiFormat;

=head1 DESCRIPTION

L<Template::Toolkit> filter plugin for L<Text::WikiFormat>

=cut

our $TAGS = {
    blocks => {
	code => qr/^: /,
    },
};

our $OPTIONS = {
    absolute_links => 1,
    implicit_links => 0,
    extended => 1,
};

=head1 METHODS

=head2 init

The initialization function.

=cut

sub init($) {
    my ($self) = @_;

    my $name = $self->{_CONFIG}->{name} || 'wiki';
    $self->{_DYNAMIC} = 1;
    $self->install_filter($name);
    return $self;
}

=head2 filter

The filter function.

=cut

sub filter($$) {
    my ($self, $raw) = @_;

    return Text::WikiFormat::format($raw, $TAGS, $OPTIONS);
}

=head1 AUTHOR

Dag-Erling Smørgrav <des@freebsd.org>

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

1;

# $FreeBSD$
