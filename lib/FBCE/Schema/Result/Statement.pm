use utf8;
package FBCE::Schema::Result::Statement;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

=head1 NAME

FBCE::Schema::Result::Statement

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

=head1 TABLE: C<statements>

=cut

__PACKAGE__->table("statements");

=head1 ACCESSORS

=head2 id

  data_type: 'integer'
  is_auto_increment: 1
  is_nullable: 0
  sequence: 'statements_id_seq'

=head2 person

  data_type: 'integer'
  is_foreign_key: 1
  is_nullable: 0

=head2 short

  data_type: 'varchar'
  is_nullable: 0
  size: 64

=head2 long

  data_type: 'text'
  is_nullable: 0

=cut

__PACKAGE__->add_columns(
  "id",
  {
    data_type         => "integer",
    is_auto_increment => 1,
    is_nullable       => 0,
    sequence          => "statements_id_seq",
  },
  "person",
  { data_type => "integer", is_foreign_key => 1, is_nullable => 0 },
  "short",
  { data_type => "varchar", is_nullable => 0, size => 64 },
  "long",
  { data_type => "text", is_nullable => 0 },
);

=head1 PRIMARY KEY

=over 4

=item * L</id>

=back

=cut

__PACKAGE__->set_primary_key("id");

=head1 UNIQUE CONSTRAINTS

=head2 C<statements_person_key>

=over 4

=item * L</person>

=back

=cut

__PACKAGE__->add_unique_constraint("statements_person_key", ["person"]);

=head1 RELATIONS

=head2 person

Type: belongs_to

Related object: L<FBCE::Schema::Result::Person>

=cut

__PACKAGE__->belongs_to(
  "person",
  "FBCE::Schema::Result::Person",
  { id => "person" },
  { is_deferrable => 1, on_delete => "CASCADE", on_update => "CASCADE" },
);


# Created by DBIx::Class::Schema::Loader v0.07022 @ 2012-05-02 18:58:53
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:4iFokZJlInUlT0SBwcqyng

use Text::WikiFormat;

sub long_html($) {
    my ($self) = @_;

    return Text::WikiFormat::format($self->long, {}, {
	implicit_links => 0, extended => 1, absolute_links => 1 });
}

1;

# $FreeBSD$
