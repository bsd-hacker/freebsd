package FBP::Controller::Poll;
use Moose;
use Storable qw(dclone);
use Try::Tiny;
use namespace::autoclean;

BEGIN { extends 'FBP::Controller'; }

=head1 NAME

FBP::Controller::Poll - Catalyst Controller

=head1 DESCRIPTION

Catalyst Controller.

=head1 METHODS

=head2 poll

Start of poll-related chain

=cut

sub poll :Chained('/') :Path :CaptureArgs(1) {
    my ($self, $c, $pid) = @_;

    $self->require_user($c);
    $c->detach('/default')
	unless $pid =~ m/^(\d+)$/;
    $pid = $1;
    my $poll = $c->model('FBP::Poll')->find($pid);
    $c->detach('/default')
	unless $poll && ($poll->active || $c->user->admin);
    $c->stash(poll => $poll);
    my $psession = ($c->session->{$pid} //= {});
    if (!$$psession{answers}) {
	# Retrieve user's existing vote, if any
	my $answers = ($$psession{answers} = {});
	foreach my $question ($poll->questions) {
	    my $votes = $c->user->votes->search({ question => $question->id });
	    $answers->{$question->id} = [ $votes->get_column('option')->all() ]
		if $votes;
	}
    }
    $$psession{qid} //= $poll->questions->first->id;
    $c->stash(title => $poll->title);
}

=head2 see

View a specific poll

=cut

sub see :Chained('poll') :PathPart('') :Args(0) {
    my ($self, $c) = @_;

    my $poll = $c->stash->{poll};
    my $questions = $poll->questions->
	search(undef, { order_by => { -asc => 'rank' } });
    $c->stash(questions => $questions);
}

=head2 vote

Vote in a poll

=cut

sub vote :Chained('poll') :Path :Args(0) {
    my ($self, $c) = @_;

    # Retrieve the poll and its list of questions
    my $poll = $c->stash->{poll};
    my $pid = $poll->id;
    my $questions = $poll->questions->
	search(undef, { order_by => { -asc => 'rank' } });
    $c->detach('/default')
	unless $poll && $questions;
    my $psession = $c->session->{$pid};
    my $answers = $$psession{answers};

    # Retrieve the current question
    my $qid = $$psession{qid};
    my $question;
    if ($qid) {
	$question = $poll->questions->find($qid);
    } else {
	$question = $questions->slice(0, 1)->first;
    }
    $c->detach('/default')
	unless $question;

    # Did the user submit any answers?
    delete($c->req->params->{answer})
	unless $poll->active;
    if ($c->req->params->{qid} ~~ $qid && $c->req->params->{answer}) {
	my $answer = $c->req->params->{answer};
	$answer = [ $answer ]
	    unless ref($answer);
	if (@$answer) {
	    try {
		$question->validate_answer(@$answer);
		$answers->{$qid} = $answer;
	    } catch {
		$$psession{vote_error} = $_;
	    };
	}
    }

    # Did the user press any of the buttons?
    if ($$psession{vote_error}) {
	# Ignore the buttons - stay on the same question
    } elsif ($c->req->params->{done}) {
	# Validate all the answers
	foreach $question ($questions->all) {
	    next unless $poll->active; # hack
	    try {
		my $answer = $answers->{$question->id};
		$question->validate_answer(@{$answer // []});
	    } catch {
		# This question was not answered correctly.  Jump to
		# it and display the corresponding error message.
		$$psession{vote_error} = $_;
		$$psession{qid} = $question->id;
		$c->response->redirect($c->request->uri);
		$c->detach();
	    };
	}
	# All questions were answered correctly.  Continue to the
	# review page.
	$c->response->redirect($c->uri_for('/poll', $pid, 'review'));
	$c->detach();
    } elsif ($c->req->params->{prev} && $question->prev) {
	$question = $question->prev;
    } elsif ($c->req->params->{next} && $question->next) {
	$question = $question->next;
    }

    # Store the current question
    $$psession{qid} = $qid = $question->id;

    # If this was a POST, redirect so reload will work
    if ($c->req->method eq 'POST') {
	$c->response->redirect($c->request->uri);
	$c->detach();
    }

    # Otherwise, display the page
    $c->stash(answer => { map { $_ => 1 } @{$answers->{$qid} // []} });
    if ($$psession{vote_error}) {
	$c->stash(error => $$psession{vote_error});
	delete($$psession{vote_error});
    }
    $c->stash(question => $question);
}

=head2 review

Review the answers and submit.

=cut

sub review :Chained('poll') :Path :Args(0) {
    my ($self, $c) = @_;

    # Retrieve poll, questions, answers
    my $poll = $c->stash->{poll};
    my $pid = $poll->id;
    my $questions = $poll->questions;
    my $psession = $c->session->{$pid};
    my $answers = $$psession{answers};
    $c->detach('/default')
	unless $poll && $questions && $answers;

    # Validate the answers
    try {
	$poll->validate_answer(%$answers)
	    if $poll->active; # hack
    } catch {
	$c->stash(error => $_);
	$c->detach();
    };

    # Did the user press any of the buttons?
    if ($$psession{vote_error}) {
	# Ignore the buttons - stay on the same question
    } elsif ($c->req->params->{confirm}) {
	try {
	    $poll->commit_answer($c->user, %$answers);
	} catch {
	    $c->stash(error => $_);
	    $c->detach();
	};
	delete($$psession{qid});
	$c->response->redirect($c->uri_for('/poll', $pid, 'done'));
	$c->detach;
    } elsif ($c->req->params->{return}) {
	delete($$psession{qid});
	$c->response->redirect($c->uri_for('/poll', $pid, 'vote'));
	$c->detach;
    }

    # If this was a POST, redirect so reload will work
    if ($c->req->method eq 'POST') {
	$c->response->redirect($c->request->uri);
	$c->detach();
    }

    # Hammer $answers into something Template::Toolkit can process
    my $options = $c->model('FBP::Option');
    $answers = dclone($answers);
    foreach my $qid (keys(%$answers)) {
	$$answers{$qid} =
	    [ map { $options->find($_) } @{$$answers{$qid}} ];
    }
    $c->stash(answers => $answers);
}

=head2 done

Thank the user for voting.

=cut

sub done :Chained('poll') :Path :Args(0) {
    my ($self, $c) = @_;

    my $poll = $c->stash->{poll};
    my $pid = $poll->id;
    delete($c->session->{$pid});
}

=head2 result

Show results.

=cut

sub result :Chained('poll') :Path :Args(0) {
    my ($self, $c) = @_;

    my $poll = $c->stash->{poll};
    $c->detach('/default')
	unless ($c->user->admin || $poll->ended);
    my $questions = $poll->questions;
    foreach my $question ($questions->all) {
	$c->log->debug("Question " . $question->id);
	my $options = $question->options;
	foreach my $option ($options->all) {
	    $c->log->debug("Option " . $option->id . ": " . $option->label);
	    $c->log->debug("Votes: " . $option->votes->count);
	}
    }
}

=head2 default

Default page.

=cut

sub default :Path {
    my ($self, $c) = @_;

    $c->detach('/default');
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
