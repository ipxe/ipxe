#!/bin/bash
#
# Copyright (C) 2010 Stefan Hajnoczi <stefanha@gmail.com>.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

if [ $# != 2 ]
then
	cat >&2 <<EOF
usage: $0 <elf-binary> <addresses-file>
Look up symbol names in <elf-binary> for function addresses from
<addresses-file>.

Example:
$0 bin/ipxe.hd.tmp fnrec.dat
EOF
	exit 1
fi

tr ' ' '\n' <"$2" | addr2line -fe "$1" | awk '(NR % 2) { print }'
