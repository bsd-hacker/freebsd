use strict;
use warnings;

use FBP;

my $app = FBP->apply_default_middlewares(FBP->psgi_app);
$app;

