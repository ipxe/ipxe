#!/usr/bin/env python
# Copyright (C) 2008 Stefan Hajnoczi <stefanha@gmail.com>.
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
import sys

try:
    import errcodedb
except ImportError:
    sys.stderr.write('Please run this first: ./build_errcodedb.py >errcodedb.py\n')
    sys.exit(1)

def to_pxenv_status(errno):
    return errno & 0xff

def to_uniq(errno):
    return (errno >> 8) & 0x1f

def to_errfile(errno):
    return (errno >> 13) & 0x7ff

def to_posix_errno(errno):
    return (errno >> 24) & 0x7f

def lookup_errno_component(defines, component):
    if component in defines:
        return defines[component]
    else:
        return '0x%x' % component

class Errcode(object):
    def __init__(self, errno):
        self.pxenv_status = to_pxenv_status(errno)
        self.uniq = to_uniq(errno)
        self.errfile = to_errfile(errno)
        self.posix_errno = to_posix_errno(errno)

    def rawstr(self):
        return 'pxenv_status=0x%x uniq=%d errfile=0x%x posix_errno=0x%x' % (self.pxenv_status, self.uniq, self.errfile, self.posix_errno)

    def prettystr(self):
        return 'pxenv_status=%s uniq=%d errfile=%s posix_errno=%s' % (
                lookup_errno_component(errcodedb.pxenv_status, self.pxenv_status),
                self.uniq,
                lookup_errno_component(errcodedb.errfile, self.errfile),
                lookup_errno_component(errcodedb.posix_errno, self.posix_errno)
                )

    def __str__(self):
        return self.prettystr()

def usage():
    sys.stderr.write('usage: %s ERROR_NUMBER\n' % sys.argv[0])
    sys.exit(1)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        usage()

    try:
        errno = int(sys.argv[1], 16)
    except ValueError:
        usage()

    print Errcode(errno)
    sys.exit(0)
