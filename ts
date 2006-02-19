#!/usr/bin/perl

=head1 NAME

ts - timestamp input

=head1 SYNOPSIS

ts [format]

=head1 DESCRIPTION

ts adds a timestamp to the beginning of each line of input

The optional format parameter controls how the timestamp is formatted,
as used by L<strftime(3)>. The default format is "%H:%M:%S".

=head1 AUTHOR

Copyright 2006 by Joey Hess <joey@kitenet.net>

Licensed under the GNU GPL.

=cut

use warnings;
use strict;
use POSIX q{strftime};

my $format="%H:%M:%S";
$format=shift if @ARGV;

$|=1;

while (<>) {
	print strftime($format, localtime)." ".$_;
}
