use strict;
use warnings;
use Test::More;


use Catalyst::Test 'FBCE';
use FBCE::Controller::Result;

ok( request('/result')->is_success, 'Request should succeed' );
done_testing();
