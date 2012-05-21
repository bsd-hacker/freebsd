package FBCE::Script::User;

use Moose;
use MooseX::Types::Common::Numeric qw/PositiveInt/;
use MooseX::Types::Moose qw/Str Bool Int/;
use FBCE;
use LWP::UserAgent;
use namespace::autoclean;

use Data::Dumper;

with 'Catalyst::ScriptRole';

has debug => (
    traits        => [qw(Getopt)],
    cmd_aliases   => 'd',
    isa           => Bool,
    is            => 'ro',
    documentation => q{Force debug mode},
);

sub cmd_list(@) {
    my ($self) = @_;

    die("too many arguments")
	if @{$self->ARGV};
    my $rs = FBCE->model('FBCE::Person');
    foreach my $person ($rs->all()) {
	print $person->login, "\n";
    }
}

sub run($) {
    my ($self) = @_;

    local $ENV{CATALYST_DEBUG} = 1
        if $self->debug;

    my $command = shift(@{$self->ARGV})
	or die("command required\n");
    if ($command eq 'list') {	
	$self->cmd_list();
    } else {
	die("unrecognized command.\n");
    }
}

__PACKAGE__->meta->make_immutable;

1;
