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

#############
# Constants #
#############


our %fields_defaults = (
    op_sys => 'FreeBSD',
    rep_platform => 'old_All',
    product => 'FreeBSD',
    version => 'unspecified',
);

our %priority_map = (
    medium => 'normal'
);

our %severity_map = (
    'non-critical' => 'normal',
    critical => 'old critical',
    serious => 'normal'
);

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

    debug_print("Body:\n" . $body, 3);

    my @body_lines = split(/\r?\n/s, $body);

    # If there are fields specified.
    my %gnats_fields;
    my $current_field;
    foreach my $line (@body_lines) {
        if ($line =~ /^>([\w-]+):\s*(.*)\s*/) {
            $current_field = lc($1);
            $gnats_fields{$current_field} = $2;
        }
        else {
            $gnats_fields{$current_field} .= "\n$line";
        }
    }

    foreach my $k (keys %gnats_fields) {
        $gnats_fields{$k} = trim ($gnats_fields{$k});
    }

    debug_print("GNATS Fields:\n" . Dumper(\%gnats_fields), 2);
    $fields{priority} = $priority_map{$gnats_fields{priority}} || $gnats_fields{priority};
    $fields{severity} = $severity_map{$gnats_fields{severity}} || $gnats_fields{severity};
    $fields{component} = $gnats_fields{category};
    # $fields{cf_type} = $gnats_fields{class};

    %fields = %{ Bugzilla::Bug::map_fields(\%fields) };

    my ($reporter) = Email::Address->parse($input_email->header('From'));
    $fields{'reporter'} = $reporter->address;
    #  $fields{'cf_originator_email'} = $reporter->address;
    $fields{'reporter'} = 'gonzo@freebsd.org';

    # Default values
    foreach my $k (keys %fields_defaults) {
        $fields{$k} = $fields_defaults{$k};
    }

    my $subject = $input_email->header('Subject');
    $fields{'short_desc'} = $gnats_fields{'synopsis'} || $subject;

    my $comment = '';


    if (defined($gnats_fields{'description'}) && $gnats_fields{'description'} =~ /\S/s) {
        $comment .= $gnats_fields{'description'};
    }

    if (defined($gnats_fields{'environment'}) && $gnats_fields{'environment'} =~ /\S/s) {
        my $envstr = $gnats_fields{'environment'};
        if ($envstr =~ /(?:FreeBSD )?([0-9]+\.[0-9\.]+-((PRE)?RELEASE|BETA[0-9]|CURRENT|STABLE))(-p\d+)?( [a-z0-9]+)?/) {
            my $version_name = $1;
            my $product = Bugzilla::Product->check($fields_defaults{product});
            eval {
                my $version_obj = Bugzilla::Version->check({ product => $product,
                                             name    => $version_name });
            };
            if ($@ eq '') {
                $fields{'version'} = $version_name;
            }
        }
        $comment .= "\n\nEnvironment:\n";
        $comment .= $gnats_fields{'environment'};
    }

    if (defined($gnats_fields{'how-to-repeat'}) && $gnats_fields{'how-to-repeat'} =~ /\S/s) {
        $comment .= "\n\nHow-To-Repeat:\n";
        $comment .= $gnats_fields{'how-to-repeat'};
    }

    if (defined($gnats_fields{'fix'})) {
        my $attachments;
        my $fix = $gnats_fields{'fix'};
        ($fix, $attachments) = parse_fix($fix);
        if ($fix =~ /\S/s) {
            $comment .= "\n\nFix:\n";
            $comment .= $fix;
        }

        $fields{'attachments'} = $attachments if (@$attachments);
    }

    $fields{'comment'} = $comment;

    debug_print("Parsed Fields:\n" . Dumper(\%fields), 2);

    return \%fields;
}

sub post_bug {
    my ($fields) = @_;
    debug_print('Posting a new bug...');

    my $user = Bugzilla->user;

    my ($retval, $non_conclusive_fields) =
      Bugzilla::User::match_field({
        'assigned_to'   => { 'type' => 'single' },
        'qa_contact'    => { 'type' => 'single' },
        'cc'            => { 'type' => 'multi'  }
      }, $fields, MATCH_SKIP_CONFIRM);

    if ($retval != USER_MATCH_SUCCESS) {
        ThrowUserError('user_match_too_many', {fields => $non_conclusive_fields});
    }

    my $bug = Bugzilla::Bug->create($fields);
    debug_print("Created bug " . $bug->id);
    return ($bug, $bug->comments->[0]);
}

sub handle_attachments {
    my ($bug, $attachments, $comment) = @_;
    return if !$attachments;
    debug_print("Handling attachments...");
    my $dbh = Bugzilla->dbh;
    $dbh->bz_start_transaction();
    my ($update_comment, $update_bug);
    foreach my $attachment (@$attachments) {
        my $data = delete $attachment->{payload};
        debug_print("Inserting Attachment: " . Dumper($attachment), 2);
        $attachment->{content_type} ||= 'application/octet-stream';
        my $ispatch = 0;
        $ispatch = 1 if ($attachment->{filename} =~ /(diff|patch)$/);
        my $obj = Bugzilla::Attachment->create({
            bug         => $bug,
            description => $attachment->{filename},
            filename    => $attachment->{filename},
            mimetype    => $attachment->{content_type},
            ispatch     => $ispatch,
            data        => $data,
        });
        # If we added a comment, and our comment does not already have a type,
        # and this is our first attachment, then we make the comment an 
        # "attachment created" comment.
        if ($comment and !$comment->type and !$update_comment) {
            $comment->set_all({ type       => CMT_ATTACHMENT_CREATED, 
                                extra_data => $obj->id });
            $update_comment = 1;
        }
        else {
            $bug->add_comment('', { type => CMT_ATTACHMENT_CREATED,
                                    extra_data => $obj->id });
            $update_bug = 1;
        }
    }
    # We only update the comments and bugs at the end of the transaction,
    # because doing so modifies bugs_fulltext, which is a non-transactional
    # table.
    $bug->update() if $update_bug;
    $comment->update() if $update_comment;
    $dbh->bz_commit_transaction();
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

Bugzilla::Hook::process('email_in_after_parse', { fields => $mail_fields });

my $attachments = delete $mail_fields->{'attachments'};

my $username = $mail_fields->{'reporter'};
# If emailsuffix is in use, we have to remove it from the email address.
if (my $suffix = Bugzilla->params->{'emailsuffix'}) {
    $username =~ s/\Q$suffix\E$//i;
}

my $user = Bugzilla::User->check($username);
Bugzilla->set_user($user);

my ($bug, $comment);
($bug, $comment) = post_bug($mail_fields);

handle_attachments($bug, $attachments, $comment);

# This is here for post_bug and handle_attachments, so that when posting a bug
# with an attachment, any comment goes out as an attachment comment.
#
# Eventually this should be sending the mail for process_bug, too, but we have
# to wait for $bug->update() to be fully used in email_in.pl first. So
# currently, process_bug.cgi does the mail sending for bugs, and this does
# any mail sending for attachments after the first one.
Bugzilla::BugMail::Send($bug->id, { changer => Bugzilla->user });
debug_print("Sent bugmail");
print "--> " . $bug->id . "\n";


__END__
