use strict;
use warnings;
use Test::More;


use Catalyst::Test 'FBP';
use FBP::Controller::Poll;

ok( request('/poll')->is_success, 'Request should succeed' );
done_testing();
