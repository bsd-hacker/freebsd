use utf8;
package FBP::View::Web;
use Moose;
use namespace::autoclean;

extends 'Catalyst::View::TT';

__PACKAGE__->config(
    TEMPLATE_EXTENSION => '.tt',
    ENCODING => 'utf-8',
    render_die => 1,
);

=head1 NAME

FBP::View::Web - TT View for FBP

=head1 DESCRIPTION

TT View for FBP.

=head1 SEE ALSO

L<FBP>

=head1 AUTHOR

Dag-Erling Smørgrav <des@freebsd.org>

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

1;

# $FreeBSD$
