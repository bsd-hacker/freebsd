use utf8;
package FBCE::Schema::Result::Person;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

=head1 NAME

FBCE::Schema::Result::Person

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

=head2 incumbent

  data_type: 'boolean'
  default_value: false
  is_nullable: 0

=head2 voted

  data_type: 'boolean'
  default_value: false
  is_nullable: 0

=head2 votes

  data_type: 'integer'
  default_value: 0
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
  "incumbent",
  { data_type => "boolean", default_value => \"false", is_nullable => 0 },
  "voted",
  { data_type => "boolean", default_value => \"false", is_nullable => 0 },
  "votes",
  { data_type => "integer", default_value => 0, is_nullable => 0 },
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

=head2 statement

Type: might_have

Related object: L<FBCE::Schema::Result::Statement>

=cut

__PACKAGE__->might_have(
  "statement",
  "FBCE::Schema::Result::Statement",
  { "foreign.person" => "self.id" },
  { cascade_copy => 0, cascade_delete => 0 },
);

=head2 votes_candidates

Type: has_many

Related object: L<FBCE::Schema::Result::Vote>

=cut

__PACKAGE__->has_many(
  "votes_candidates",
  "FBCE::Schema::Result::Vote",
  { "foreign.candidate" => "self.id" },
  { cascade_copy => 0, cascade_delete => 0 },
);

=head2 votes_voters

Type: has_many

Related object: L<FBCE::Schema::Result::Vote>

=cut

__PACKAGE__->has_many(
  "votes_voters",
  "FBCE::Schema::Result::Vote",
  { "foreign.voter" => "self.id" },
  { cascade_copy => 0, cascade_delete => 0 },
);


# Created by DBIx::Class::Schema::Loader v0.07024 @ 2012-05-21 23:49:53
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:QtgEo2NXwa8v6FRHUuQ/Lg

use Crypt::SaltedHash;
use Digest::MD5 qw(md5_hex);

#
# Change the password.
#
sub set_password($$) {
    my ($self, $password) = @_;

    my $csh = new Crypt::SaltedHash(algorithm => 'SHA-1');
    $csh->add($password);
    $self->set_column(password => $csh->generate());
    $self->update()
	if $self->in_storage();
}

#
# Check the password.
#
sub check_password($$) {
    my ($self, $password) = @_;

    return Crypt::SaltedHash->validate($self->password, $password);
}

#
# Pretty name
#
sub name($) {
    my ($self) = @_;

    return $self->realname || ($self->login . '@freebsd.org');
}

#
# Commit votes
#
sub commit($) {
    my ($self) = @_;

    my $schema = $self->result_source->schema;
    $schema->txn_do(sub {
	my $votes = $self->votes_voters;
	while (my $vote = $votes->next) {
	    $vote->candidate->votes++;
	    $vote->delete;
	}
    });
}

#
# Email address
#
sub email($) {
    my ($self) = @_;

    return $self->login . "\@freebsd.org";
}

#
# Gravatar URL
#
sub gravatar($;$) {
    my ($self, $scheme) = @_;

    my $md5 = md5_hex($self->email);
    if ($scheme eq 'https') {
	return "https://secure.gravatar.com/avatar/$md5";
    } else {
	return "http://www.gravatar.com/avatar/$md5";
    }
}

1;

# $FreeBSD$
