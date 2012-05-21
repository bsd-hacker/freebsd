use strict;
use warnings;
use Test::More;

BEGIN { use_ok 'Catalyst::Test', 'FBCE' }
BEGIN { use_ok 'FBCE::Controller::Vote' }

ok( request('/vote')->is_success, 'Request should succeed' );
done_testing();
