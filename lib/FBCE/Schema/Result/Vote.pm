use utf8;
package FBCE::Schema::Result::Vote;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

=head1 NAME

FBCE::Schema::Result::Vote

=cut

use strict;
use warnings;

use base 'DBIx::Class::Core';

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

=head2 candidate

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
  "candidate",
  { data_type => "integer", is_foreign_key => 1, is_nullable => 0 },
);

=head1 PRIMARY KEY

=over 4

=item * L</id>

=back

=cut

__PACKAGE__->set_primary_key("id");

=head1 UNIQUE CONSTRAINTS

=head2 C<votes_voter_candidate_key>

=over 4

=item * L</voter>

=item * L</candidate>

=back

=cut

__PACKAGE__->add_unique_constraint("votes_voter_candidate_key", ["voter", "candidate"]);

=head1 RELATIONS

=head2 candidate

Type: belongs_to

Related object: L<FBCE::Schema::Result::Person>

=cut

__PACKAGE__->belongs_to(
  "candidate",
  "FBCE::Schema::Result::Person",
  { id => "candidate" },
  { is_deferrable => 1, on_delete => "CASCADE", on_update => "CASCADE" },
);

=head2 voter

Type: belongs_to

Related object: L<FBCE::Schema::Result::Person>

=cut

__PACKAGE__->belongs_to(
  "voter",
  "FBCE::Schema::Result::Person",
  { id => "voter" },
  { is_deferrable => 1, on_delete => "CASCADE", on_update => "CASCADE" },
);


# Created by DBIx::Class::Schema::Loader v0.07022 @ 2012-05-02 18:58:53
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:gBIEgDR5kVbXd4B6h3LRQg

1;

# $FreeBSD$
