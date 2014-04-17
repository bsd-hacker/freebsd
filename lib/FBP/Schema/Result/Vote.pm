use utf8;
package FBP::Schema::Result::Vote;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

=head1 NAME

FBP::Schema::Result::Vote

=cut

use strict;
use warnings;

use Moose;
use MooseX::NonMoose;
use MooseX::MarkAsMethods autoclean => 1;
extends 'DBIx::Class::Core';

=head1 COMPONENTS LOADED

=over 4

=item * L<DBIx::Class::InflateColumn::DateTime>

=back

=cut

__PACKAGE__->load_components("InflateColumn::DateTime");

=head1 TABLE: C<votes>

=cut

__PACKAGE__->table("votes");

=head1 ACCESSORS

=head2 id

  data_type: 'integer'
  is_auto_increment: 1
  is_nullable: 0
  sequence: 'votes_id_seq'

=head2 voter

  data_type: 'integer'
  is_foreign_key: 1
  is_nullable: 0

=head2 question

  data_type: 'integer'
  is_foreign_key: 1
  is_nullable: 0

=head2 option

  data_type: 'integer'
  is_foreign_key: 1
  is_nullable: 0

=cut

__PACKAGE__->add_columns(
  "id",
  {
    data_type         => "integer",
    is_auto_increment => 1,
    is_nullable       => 0,
    sequence          => "votes_id_seq",
  },
  "voter",
  { data_type => "integer", is_foreign_key => 1, is_nullable => 0 },
  "question",
  { data_type => "integer", is_foreign_key => 1, is_nullable => 0 },
  "option",
  { data_type => "integer", is_foreign_key => 1, is_nullable => 0 },
);

=head1 PRIMARY KEY

=over 4

=item * L</id>

=back

=cut

__PACKAGE__->set_primary_key("id");

=head1 UNIQUE CONSTRAINTS

=head2 C<votes_voter_option_key>

=over 4

=item * L</voter>

=item * L</option>

=back

=cut

__PACKAGE__->add_unique_constraint("votes_voter_option_key", ["voter", "option"]);

=head1 RELATIONS

=head2 option

Type: belongs_to

Related object: L<FBP::Schema::Result::Option>

=cut

__PACKAGE__->belongs_to(
  "option",
  "FBP::Schema::Result::Option",
  { id => "option" },
  { is_deferrable => 0, on_delete => "CASCADE", on_update => "CASCADE" },
);

=head2 question

Type: belongs_to

Related object: L<FBP::Schema::Result::Question>

=cut

__PACKAGE__->belongs_to(
  "question",
  "FBP::Schema::Result::Question",
  { id => "question" },
  { is_deferrable => 0, on_delete => "CASCADE", on_update => "CASCADE" },
);

=head2 voter

Type: belongs_to

Related object: L<FBP::Schema::Result::Person>

=cut

__PACKAGE__->belongs_to(
  "voter",
  "FBP::Schema::Result::Person",
  { id => "voter" },
  { is_deferrable => 0, on_delete => "CASCADE", on_update => "CASCADE" },
);


# Created by DBIx::Class::Schema::Loader v0.07039 @ 2014-04-16 20:57:55
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:TIV5w+lodXu0vgk/zqosbA

=head2 poll

Returns the poll in which this vote was cast.

=cut

sub poll($) {
    my ($self) = @_;

    return $self->question->poll;
}

=head1 AUTHOR

Dag-Erling Smørgrav <des@freebsd.org>

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

__PACKAGE__->meta->make_immutable;

1;

# $FreeBSD$
