#!/usr/bin/perl -w

use bytes;

local $/;
$_ = <>;
print length($_);
exit;
