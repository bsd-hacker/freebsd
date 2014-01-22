#!/usr/local/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Inbound Email System.
#
# The Initial Developer of the Original Code is Akamai Technologies, Inc.
# Portions created by Akamai are Copyright (C) 2006 Akamai Technologies, 
# Inc. All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;
use warnings;

# MTAs may call this script from any directory, but it should always
# run from this one so that it can find its modules.
use Cwd qw(abs_path);
use File::Basename qw(dirname);
BEGIN {
    # Untaint the abs_path.
    my ($a) = abs_path($0) =~ /^(.*)$/;
    chdir dirname($a);
}

use lib qw(/usr/local/www/bugs42.freebsd.org/lib/ /usr/local/www/bugs42.freebsd.org/);

use Data::Dumper;
use Email::Address;
use Email::Reply qw(reply);
use Email::MIME;
use Email::MIME::Attachment::Stripper;
use Getopt::Long qw(:config bundling);
use Pod::Usage;
use Encode;
use Scalar::Util qw(blessed);

use Bugzilla;
use Bugzilla::Attachment;
use Bugzilla::Bug;
use Bugzilla::BugMail;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Mailer;
use Bugzilla::Token;
use Bugzilla::User;
use Bugzilla::Util;
use Bugzilla::Hook;

# $input_email is a global so that it can be used in die_handler.
our ($input_email, %switch);

####################
# Main Subroutines #
####################

sub parse_mail {
    my ($mail_text) = @_;
    debug_print('Parsing Email');
    $input_email = Email::MIME->new($mail_text);
    
    my %fields = %{ $switch{'default'} || {} };
    Bugzilla::Hook::process('email_in_before_parse', { mail => $input_email,
                                                       fields => \%fields });

    my $body = get_body($input_email);

    # debug_print("Body:\n" . $body, 3);

    my @body_lines = split(/\r?\n/s, $body);

    my @comment_lines = ();
    foreach my $l (@body_lines) {
        last if ($l =~ /^Modified:.*\/.*/);
        push @comment_lines, $l;
    }
    my $comment = join "\n", @comment_lines;
    $fields{'comment'} = $comment;
    if ($comment =~ /\n\s*PR:\s*(?:\w+\/)?(\d+)/) {
        $fields{'bug_id'} = $1;
    }

    debug_print("Parsed Fields:\n" . Dumper(\%fields), 2);

    return \%fields;
}

######################
# Helper Subroutines #
######################

sub debug_print {
    my ($str, $level) = @_;
    $level ||= 1;
    print STDERR "$str\n" if $level <= $switch{'verbose'};
}

sub get_body {
    my ($email) = @_;

    my $ct = $email->content_type || 'text/plain';
    debug_print("Splitting Body and Attachments [Type: $ct]...");

    my $body;
    if ($ct =~ /^multipart\/(alternative|signed)/i) {
        $body = get_text_alternative($email);
    }
    else {
        my $stripper = new Email::MIME::Attachment::Stripper(
            $email, force_filename => 1);
        my $message = $stripper->message;
        $body = get_text_alternative($message);
    }

    return $body;
}

sub parse_fix {
    my ($fix) = @_;
    my $stripped_fix = '';
    my $attachments = [];
    my $attachment;
    my @fix_lines = split(/\r?\n/s, $fix);
    foreach my $line (@fix_lines) {
        if ($line =~ /^\s*--{1,8}\s?([A-Za-z0-9-_.,:%]+) (begins|starts) here\s?--+\s*/mi) {
            push @$attachments, $attachment if (defined($attachment));
            $attachment = {
                'payload' => '',
                'filename' => $1,
                'content_type' => 'text/plain',
            };
        }
        elsif ($line =~ /^\s*--{1,8}\s?([A-Za-z0-9-_.,:%]+) ends here\s?--+\s*\n/mi) {
            push @$attachments, $attachment if (defined($attachment));
            $attachment = undef;
        }
        elsif ($line =~ /^# This is a shell archive/) {
            push @$attachments, $attachment if (defined($attachment));
            $attachment = {
                'payload' => "$line\n",
                'filename' => 'file.shar',
                'content_type' => 'application/x-shar',
            };
        }
        elsif (($line =~ /^exit$/) && defined($attachment) && ($attachment->{content_type} =~/x-shar/)) {
            $attachment->{'payload'} .= "$line\n";
            push @$attachments, $attachment if (defined($attachment));
            $attachment = undef;
        }

        elsif (defined($attachment)) {
            $attachment->{'payload'} .= "$line\n";
        }
        else {
            $stripped_fix .= "$line\n";
        }
    }
    push @$attachments, $attachment if (defined($attachment));
    return ($stripped_fix, $attachments)
}

sub get_text_alternative {
    my ($email) = @_;

    my @parts = $email->parts;
    my $body;
    foreach my $part (@parts) {
        my $ct = $part->content_type || 'text/plain';
        my $charset = 'iso-8859-1';
        # The charset may be quoted.
        if ($ct =~ /charset="?([^;"]+)/) {
            $charset= $1;
        }
        debug_print("Part Content-Type: $ct", 2);
        debug_print("Part Character Encoding: $charset", 2);
        if (!$ct || $ct =~ /^text\/plain/i) {
            $body = $part->body;
            if (Bugzilla->params->{'utf8'} && !utf8::is_utf8($body)) {
                $body = Encode::decode($charset, $body);
            }
            last;
        }
    }

    if (!defined $body) {
        # Note that this only happens if the email does not contain any
        # text/plain parts. If the email has an empty text/plain part,
        # you're fine, and this message does NOT get thrown.
        ThrowUserError('email_no_text_plain');
    }

    return $body;
}

sub html_strip {
    my ($var) = @_;
    # Trivial HTML tag remover (this is just for error messages, really.)
    $var =~ s/<[^>]*>//g;
    # And this basically reverses the Template-Toolkit html filter.
    $var =~ s/\&amp;/\&/g;
    $var =~ s/\&lt;/</g;
    $var =~ s/\&gt;/>/g;
    $var =~ s/\&quot;/\"/g;
    $var =~ s/&#64;/@/g;
    # Also remove undesired newlines and consecutive spaces.
    $var =~ s/[\n\s]+/ /gms;
    return $var;
}

sub die_handler {
    my ($msg) = @_;

    # In Template-Toolkit, [% RETURN %] is implemented as a call to "die".
    # But of course, we really don't want to actually *die* just because
    # the user-error or code-error template ended. So we don't really die.
    return if blessed($msg) && $msg->isa('Template::Exception')
              && $msg->type eq 'return';

    # If this is inside an eval, then we should just act like...we're
    # in an eval (instead of printing the error and exiting).
    die(@_) if $^S;

    # We can't depend on the MTA to send an error message, so we have
    # to generate one properly.
    if ($input_email) {
       $msg =~ s/at .+ line.*$//ms;
       $msg =~ s/^Compilation failed in require.+$//ms;
       $msg = html_strip($msg);
       my $from = Bugzilla->params->{'mailfrom'};
       my $reply = reply(to => $input_email, from => $from, top_post => 1, 
                         body => "$msg\n");
       # MessageToMTA($reply->as_string);
    }
    print STDERR "$msg\n";
    # We exit with a successful value, because we don't want the MTA
    # to *also* send a failure notice.
    exit;
}

###############
# Main Script #
###############

$SIG{__DIE__} = \&die_handler;

GetOptions(\%switch, 'help|h', 'verbose|v+', 'default=s%', 'override=s%');
$switch{'verbose'} ||= 0;

# Print the help message if that switch was selected.
pod2usage({-verbose => 0, -exitval => 1}) if $switch{'help'};

Bugzilla->usage_mode(USAGE_MODE_EMAIL);

my @mail_lines = <STDIN>;
my $mail_text = join("", @mail_lines);
my $mail_fields = parse_mail($mail_text);

exit(0) unless(defined($mail_fields->{'bug_id'}));

my $bug_id = $mail_fields->{'bug_id'};
my $comment = $mail_fields->{'comment'};
my $bug = Bugzilla::Bug->check($bug_id);

my $username = 'dfilter@freebsd.org';
my $user = Bugzilla::User->check($username);
Bugzilla->set_user($user);

$bug->add_comment($comment);
my $dbh = Bugzilla->dbh;
$dbh->bz_start_transaction();

$bug->update();
$dbh->bz_commit_transaction();

__END__
