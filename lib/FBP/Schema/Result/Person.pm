use utf8;
package FBP::Schema::Result::Person;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

=head1 NAME

FBP::Schema::Result::Person

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

=head1 TABLE: C<persons>

=cut

__PACKAGE__->table("persons");

=head1 ACCESSORS

=head2 id

  data_type: 'integer'
  is_auto_increment: 1
  is_nullable: 0
  sequence: 'persons_id_seq'

=head2 login

  data_type: 'text'
  is_nullable: 0
  original: {data_type => "varchar"}

=head2 realname

  data_type: 'text'
  is_nullable: 1
  original: {data_type => "varchar"}

=head2 password

  data_type: 'text'
  default_value: '*'
  is_nullable: 0
  original: {data_type => "varchar"}

=head2 admin

  data_type: 'boolean'
  default_value: false
  is_nullable: 0

=head2 active

  data_type: 'boolean'
  default_value: false
  is_nullable: 0

=cut

__PACKAGE__->add_columns(
  "id",
  {
    data_type         => "integer",
    is_auto_increment => 1,
    is_nullable       => 0,
    sequence          => "persons_id_seq",
  },
  "login",
  {
    data_type   => "text",
    is_nullable => 0,
    original    => { data_type => "varchar" },
  },
  "realname",
  {
    data_type   => "text",
    is_nullable => 1,
    original    => { data_type => "varchar" },
  },
  "password",
  {
    data_type     => "text",
    default_value => "*",
    is_nullable   => 0,
    original      => { data_type => "varchar" },
  },
  "admin",
  { data_type => "boolean", default_value => \"false", is_nullable => 0 },
  "active",
  { data_type => "boolean", default_value => \"false", is_nullable => 0 },
);

=head1 PRIMARY KEY

=over 4

=item * L</id>

=back

=cut

__PACKAGE__->set_primary_key("id");

=head1 UNIQUE CONSTRAINTS

=head2 C<persons_login_key>

=over 4

=item * L</login>

=back

=cut

__PACKAGE__->add_unique_constraint("persons_login_key", ["login"]);

=head1 RELATIONS

=head2 polls

Type: has_many

Related object: L<FBP::Schema::Result::Poll>

=cut

__PACKAGE__->has_many(
  "polls",
  "FBP::Schema::Result::Poll",
  { "foreign.owner" => "self.id" },
  { cascade_copy => 0, cascade_delete => 0 },
);

=head2 votes

Type: has_many

Related object: L<FBP::Schema::Result::Vote>

=cut

__PACKAGE__->has_many(
  "votes",
  "FBP::Schema::Result::Vote",
  { "foreign.voter" => "self.id" },
  { cascade_copy => 0, cascade_delete => 0 },
);


# Created by DBIx::Class::Schema::Loader v0.07039 @ 2014-04-16 20:57:55
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:19kISwX2Afx2WCQPAB8akw

use Crypt::SaltedHash;

=head2 set_password

Set this person's password to the specified string.

=cut

sub set_password($$) {
    my ($self, $password) = @_;

    if ($password !~ m/^[[:print:]]{8,}$/a || $password !~ m/[0-9]/a ||
	$password !~ m/[A-Z]/a || $password !~ m/[a-z]/a) {
	die("Your password must be at least 8 characters long and contain" .
	    " at least one upper-case letter, one lower-case letter and" .
	    " one digit.\n");
    }
    my $csh = new Crypt::SaltedHash(algorithm => 'SHA-256');
    $csh->add($password);
    $self->set_column(password => $csh->generate());
    $self->update()
	if $self->in_storage();
}

=head2 check_password

Verify that the specified string matches the user's password.

=cut

sub check_password($$) {
    my ($self, $password) = @_;

    return Crypt::SaltedHash->validate($self->password, $password);
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
