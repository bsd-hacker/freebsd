use strict;
use warnings;
use Test::More;

BEGIN { use_ok 'Catalyst::Test', 'FBCE' }
BEGIN { use_ok 'FBCE::Controller::Run' }

ok( request('/run')->is_success, 'Request should succeed' );
done_testing();
