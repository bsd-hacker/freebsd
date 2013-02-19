#!/usr/bin/perl
#
# Script to create HTML page of annotated sets of bug reports.
#
# author: linimon
# thanks to: pgollucci for perl help
#

require '/home/gnats/tools/cgi-lib.pl';
require '/home/gnats/tools/cgi-style.pl';
require '/home/gnats/tools/query-pr-common.pl';
require 'getopts.pl';

$query_pr_ref="http://www.freebsd.org/cgi/query-pr.cgi";

local($filename)=$ARGV[0];
local($html_mode)=$ARGV[1];

$wrote_header = 0;
$accumulating_prs = 0;

open my $fh, '<', "$filename" or die "Can't open [$filename] because [$!]\n";

while (my $line = <$fh>) {

  chomp $line;

  if ($line =~ /^\d+/) {
    # line is a PR number.  Add it to the current PR list.
    $query_args=$line . " -x";
    @prs=&read_gnats($query_args);
    $accumulating_prs = 1;
  }
  else {
    # line is text.

    # if HTML mode, 1st text line is the HTML title (hack)
    if (! $wrote_header ) {
      if ($html_mode ) {
        print &noninteractive_html_header($line);
        print "<p>\n";
        print "Date generated: " . gmtime() . " GMT\n";
        print "</p>\n";
      } else {
        print "Date generated: " . gmtime() . " GMT\n";
        print "\n";
        print $line . "\n";
        print "\n";
      }
      $wrote_header = 1;
    } else {
      if ($line =~ /%QUERY-PR/) {
        # use the rest of the line as arguments
        $line =~ s/%QUERY-PR//;
        # hack: add a macro for "tag" -- not supported by query-pr
        $line =~ s/--tag (\S+)/-t \'\\[$1\\]\'/ if ($line =~ /--tag /);
	# Only add -x if noone gave --state=closed/-s closed
        if ($line !~ /-s\s+\S*closed/ and $line !~ /--state=\S*closed/) {
          $line .= " -x";
        }
        $query_args=$line;
        # print "<br>query_args are " . $query_args . "<br>\n";
        @prs=&read_gnats($query_args);
        $accumulating_prs = 1;
      } else {
        # just a regular text line
        if ($accumulating_prs) {
          # need to write out the accumulated PRs first.
          @prs=reverse(@prs);
          &printcnt(&gnats_summary(1, $html_mode, \@prs, $query_pr_ref));
          # do not carry those over to the next block
          @prs = ();
          $accumulating_prs = 0;
        }
        # any text line other than the first, or %QUERY-PR, is echoed verbatim
        print $line . "\n";
      }
    }
  }
}
close $fh or die "Can't close [$filename] because [$!]\n";

if ($accumulating_prs) {
  # write out last block
  @prs=reverse(@prs);
  &printcnt(&gnats_summary(1, $html_mode, \@prs, $query_pr_ref));
}

if ($html_mode ) {
  print &html_footer;
}

exit(0);
