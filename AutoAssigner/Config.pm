package Bugzilla::Extension::AutoAssigner;
use strict;
use constant NAME => 'AutoAssigner';
use constant REQUIRED_MODULES => [
    {
        package => 'Data-Dumper',
        module  => 'Data::Dumper',
        version => 0,
    },
];

__PACKAGE__->NAME;
