#!/usr/bin/perl
#
# $FreeBSD$
#

use inc::Module::Install;
use Module::Install::Catalyst;

name 'FBCE';
all_from 'lib/FBCE.pm';

requires 'Catalyst::Action::RenderView';
requires 'Catalyst::Authentication::Store::DBIx::Class';
requires 'Catalyst::Model::DBIC::Schema';
requires 'Catalyst::Plugin::Authentication';
requires 'Catalyst::Plugin::ConfigLoader';
requires 'Config::General';
requires 'Catalyst::Plugin::DateTime';
requires 'MooseX::Types::DateTime::MoreCoercions';
requires 'MooseX::Types::Common::Numeric';
requires 'Catalyst::Plugin::Session';
requires 'Catalyst::Plugin::Session::State::Cookie';
requires 'Catalyst::Plugin::Session::Store::FastMmap';
requires 'Catalyst::Plugin::Static::Simple';
requires 'Catalyst::Runtime' => '5.90053';
requires 'Catalyst::View::TT';
requires 'Crypt::SaltedHash';
requires 'Text::WikiFormat';

requires 'Moose';
requires 'namespace::autoclean';
test_requires 'Test::More' => '0.88';
catalyst;

install_script glob('script/*.pl');
auto_install;
WriteAll;
