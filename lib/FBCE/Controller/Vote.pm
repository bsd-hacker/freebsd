package FBCE::Controller::Vote;
use Moose;
use namespace::autoclean;

BEGIN { extends 'Catalyst::Controller' }

=head1 NAME

FBCE::Controller::Vote - Catalyst Controller

=head1 DESCRIPTION

Catalyst Controller.

=head1 METHODS

=cut

=head2 index

=cut

sub index :Path :Args(0) {
    my ($self, $c) = @_;

    my $user = $c->user->get_object();
    $c->stash(user => $user);
    if ($c->stash->{'phase'} != 0) {
	return;
    }
    my $p = $c->req->params;
    if ($p->{cancel}) {
	$c->res->redirect($c->uri_for('/'));
	$c->detach();
    }
    my $candidates = $c->model('FBCE::Statement')->
	search_related('person', {}, { order_by => 'login' });
    my %voted_for;
    my $error;
    if ($p->{vote}) {
	my %vote_for;
	while (my $candidate = $candidates->next) {
	    if (exists $p->{"vote_for_" . $candidate->login}) {
		$vote_for{$candidate->login} = $candidate;
		$voted_for{$candidate->login} = 1;
	    }
	}
	$candidates->reset;
	if (scalar keys %vote_for > $c->stash->{'max_votes'}) {
	    $error = "You can only vote for $c->stash->{'max_votes'} candidates.";
	} else {
	    my $schema = $user->result_source->schema;
	    $schema->txn_do(sub {
		$user->votes_voters->delete();
		while (my ($login, $candidate) = each %vote_for) {
		    $user->votes_voters->create({ candidate => $candidate });
		}
	    });
	    if ($@) {
		$error = "Database error!";
	    } else {
		$c->stash(vote_ok => 1);
	    }
	}
    } else {
	my $votes = $user->votes_voters;
	while (my $vote = $votes->next) {
	    $voted_for{$vote->candidate->login} = 1;
	}
    }
    $c->stash(error => $error);
    $c->stash(candidates => $candidates);
    $c->stash(max_votes => $c->stash->{'max_votes'});
    $c->stash(voted_for => \%voted_for);
}

# sub commit :Local :Args(0) {
#     my ($self, $c) = @_;

#     $c->authenticate();
#     my $user = $c->user->get_object();
#     if ($c->stash->{'phase'} != 0) {
# 	$c->res->redirect($c->uri_for('/vote'));
# 	$c->detach();
#     }
#     my $p = $c->req->params;
#     if ($p->{cancel}) {
# 	$c->res->redirect($c->uri_for('/vote'));
# 	$c->detach();
#     }
#     if ($p->{commit}) {
# 	$user->commit()
# 	    or die("failed to commit");
# 	$c->res->redirect($c->uri_for('/vote'));
# 	$c->detach();
#     }
#     $c->stash(user => $user);
# }


=head1 AUTHOR

Dag-Erling Smørgrav

=head1 LICENSE

This library is free software. You can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

__PACKAGE__->meta->make_immutable;

1;
