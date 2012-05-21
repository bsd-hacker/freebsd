use strict;
use warnings;
use Test::More;

BEGIN { use_ok 'Catalyst::Test', 'FBCE' }
BEGIN { use_ok 'FBCE::Controller::See' }

ok( request('/see')->is_success, 'Request should succeed' );
done_testing();
