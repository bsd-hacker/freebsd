use utf8;
package FBP::Schema::Result::Question;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

=head1 NAME

FBP::Schema::Result::Question

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

=head1 TABLE: C<questions>

=cut

__PACKAGE__->table("questions");

=head1 ACCESSORS

=head2 id

  data_type: 'integer'
  is_auto_increment: 1
  is_nullable: 0
  sequence: 'questions_id_seq'

=head2 poll

  data_type: 'integer'
  is_foreign_key: 1
  is_nullable: 0

=head2 rank

  data_type: 'integer'
  is_nullable: 0

=head2 short

  data_type: 'varchar'
  is_nullable: 0
  size: 256

=head2 long

  data_type: 'text'
  is_nullable: 0

=head2 min_options

  data_type: 'integer'
  default_value: 1
  is_nullable: 0

=head2 max_options

  data_type: 'integer'
  default_value: 1
  is_nullable: 0

=cut

__PACKAGE__->add_columns(
  "id",
  {
    data_type         => "integer",
    is_auto_increment => 1,
    is_nullable       => 0,
    sequence          => "questions_id_seq",
  },
  "poll",
  { data_type => "integer", is_foreign_key => 1, is_nullable => 0 },
  "rank",
  { data_type => "integer", is_nullable => 0 },
  "short",
  { data_type => "varchar", is_nullable => 0, size => 256 },
  "long",
  { data_type => "text", is_nullable => 0 },
  "min_options",
  { data_type => "integer", default_value => 1, is_nullable => 0 },
  "max_options",
  { data_type => "integer", default_value => 1, is_nullable => 0 },
);

=head1 PRIMARY KEY

=over 4

=item * L</id>

=back

=cut

__PACKAGE__->set_primary_key("id");

=head1 UNIQUE CONSTRAINTS

=head2 C<questions_poll_rank_key>

=over 4

=item * L</poll>

=item * L</rank>

=back

=cut

__PACKAGE__->add_unique_constraint("questions_poll_rank_key", ["poll", "rank"]);

=head1 RELATIONS

=head2 options

Type: has_many

Related object: L<FBP::Schema::Result::Option>

=cut

__PACKAGE__->has_many(
  "options",
  "FBP::Schema::Result::Option",
  { "foreign.question" => "self.id" },
  { cascade_copy => 0, cascade_delete => 0 },
);

=head2 poll

Type: belongs_to

Related object: L<FBP::Schema::Result::Poll>

=cut

__PACKAGE__->belongs_to(
  "poll",
  "FBP::Schema::Result::Poll",
  { id => "poll" },
  { is_deferrable => 0, on_delete => "CASCADE", on_update => "CASCADE" },
);

=head2 votes

Type: has_many

Related object: L<FBP::Schema::Result::Vote>

=cut

__PACKAGE__->has_many(
  "votes",
  "FBP::Schema::Result::Vote",
  { "foreign.question" => "self.id" },
  { cascade_copy => 0, cascade_delete => 0 },
);


# Created by DBIx::Class::Schema::Loader v0.07039 @ 2014-04-16 20:57:55
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:I/1G7NpDuffuLD3XnoJLpw

=head2 validate_answer

Validates an answer to this question and dies if it is not valid.

=cut

sub validate_answer($@) {
    my ($self, @answer) = @_;

    if (!@answer && $self->min_options > 0) {
	die("You did not answer this question.\n");
    } elsif (@answer < $self->min_options) {
	die("You must select at least " . $self->min_options . " options\n");
    } elsif (@answer > $self->max_options) {
	if ($self->max_options == 1) {
	    die("You may only select one option.\n");
	} else {
	    die("You may select at most " . $self->max_options . " options.");
	}
    }
    foreach my $oid (@answer) {
	$self->options->find($oid)
	    or die("Option $oid is not a valid answer to this question\n");
    }
}

=head2 commit_answer

Registers a voter's answer to this question.

=cut

sub commit_answer($$@) {
    my ($self, $voter, @answer) = @_;

    print STDERR "Question ", $self->id, " commit_answer\n";
    $voter->votes->search({ question => $self->id })->delete();
    foreach my $oid (@answer) {
	$voter->votes->create({ question => $self->id, option => $oid });
    }
}

=head2 prev

Returns the previous question by rank.

=cut

sub prev($) {
    my ($self) = @_;

    my $questions = $self->poll->questions->
	search({ rank => { '<', $self->rank } },
	       { order_by => { -desc => 'id' } })
	or return undef;
    return $questions->slice(0, 1)->first;
}

=head2 prev

Returns the next question by rank.

=cut

sub next($) {
    my ($self) = @_;

    my $questions = $self->poll->questions->
	search({ rank => { '>', $self->rank } },
	       { order_by => { -asc => 'id' } })
	or return undef;
    return $questions->slice(0, 1)->first;
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
