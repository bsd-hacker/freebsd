use utf8;
package FBP::Schema::Result::Poll;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

=head1 NAME

FBP::Schema::Result::Poll

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

=head1 TABLE: C<polls>

=cut

__PACKAGE__->table("polls");

=head1 ACCESSORS

=head2 id

  data_type: 'integer'
  is_auto_increment: 1
  is_nullable: 0
  sequence: 'polls_id_seq'

=head2 owner

  data_type: 'integer'
  is_foreign_key: 1
  is_nullable: 0

=head2 title

  data_type: 'varchar'
  is_nullable: 0
  size: 64

=head2 starts

  data_type: 'timestamp'
  is_nullable: 0

=head2 ends

  data_type: 'timestamp'
  is_nullable: 0

=head2 synopsis

  data_type: 'varchar'
  is_nullable: 0
  size: 256

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
    sequence          => "polls_id_seq",
  },
  "owner",
  { data_type => "integer", is_foreign_key => 1, is_nullable => 0 },
  "title",
  { data_type => "varchar", is_nullable => 0, size => 64 },
  "starts",
  { data_type => "timestamp", is_nullable => 0 },
  "ends",
  { data_type => "timestamp", is_nullable => 0 },
  "synopsis",
  { data_type => "varchar", is_nullable => 0, size => 256 },
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

=head2 C<polls_title_key>

=over 4

=item * L</title>

=back

=cut

__PACKAGE__->add_unique_constraint("polls_title_key", ["title"]);

=head1 RELATIONS

=head2 owner

Type: belongs_to

Related object: L<FBP::Schema::Result::Person>

=cut

__PACKAGE__->belongs_to(
  "owner",
  "FBP::Schema::Result::Person",
  { id => "owner" },
  { is_deferrable => 0, on_delete => "CASCADE", on_update => "CASCADE" },
);

=head2 questions

Type: has_many

Related object: L<FBP::Schema::Result::Question>

=cut

__PACKAGE__->has_many(
  "questions",
  "FBP::Schema::Result::Question",
  { "foreign.poll" => "self.id" },
  { cascade_copy => 0, cascade_delete => 0 },
);


# Created by DBIx::Class::Schema::Loader v0.07039 @ 2014-04-16 20:57:55
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:wB2dAarq+nsbZ5Ljfsil7Q

use DateTime;

=index2 started

True if the poll had, has or will have started at the specified date
and time.

=cut

sub started($;$) {
    my ($self, $when) = @_;

    $when //= DateTime->now();
    return DateTime->compare($when, $self->starts) >= 0;
}

=index2 ended

True if the poll had, has or will have ended at the specified date and
time.

=cut

sub ended($;$) {
    my ($self, $when) = @_;

    $when //= DateTime->now();
    return DateTime->compare($when, $self->ends) >= 0;
}

=index2 active

True if the poll was, is or will be active at the specified date and
time.

=cut

sub active($;$) {
    my ($self, $when) = @_;

    $when //= DateTime->now();
    return $self->started($when) && !$self->ended($when);
}

=head2 votes

Return votes cast in this poll.  In list context, returns a list of
votes.  In scalar context, returns a resultset.

=cut

sub votes($) {
    my ($self) = @_;

    return wantarray() ? $self->votes_rs->all : $self->votes_rs;
}

=head2 votes_rs

Return votes cast in this poll as a resultset.

=cut

sub votes_rs($) {
    my ($self) = @_;

    return $self->questions->search_related_rs('votes');
}

=head2 validate_answer

Validates an answer to this poll and dies if it is not valid.

=cut

sub validate_answer($%) {
    my ($self, %answers) = @_;

    my %questions = map({ $_->id => $_ } $self->questions);
    # Verify that all questions have received valid answers
    foreach my $qid (keys %questions) {
	if (!defined($answers{$qid})) {
	    die("Question $qid has not been answered.\n");
	} elsif (ref($answers{$qid}) ne 'ARRAY') {
	    die("Internal error\n");
	} else {
	    $questions{$qid}->validate_answer(@{$answers{$qid}});
	}
    }
    # Verify that there are no answers without a matching question
    if (!(keys(%answers) ~~ keys(%questions))) {
	die("Too many answers\n");
    }
}

=head2 commit_answer

Commits an answer to this poll.

=cut

sub commit_answer($$%) {
    my ($self, $voter, %answers) = @_;

    die("You cannot vote in an inactive poll\n")
	unless $self->active;
    foreach my $question ($self->questions) {
	$question->commit_answer($voter, @{$answers{$question->id}});
    }
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
