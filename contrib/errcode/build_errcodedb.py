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
import re

pxenv_status_files = ('../../src/include/errno.h', )
errfile_files = ('../../src/include/gpxe/errfile.h',
            '../../src/arch/i386/include/bits/errfile.h')
posix_errno_files = ('../../src/include/errno.h', )

PXENV_STATUS_RE = re.compile(r'^#define\s+(PXENV_STATUS_[^\s]+)\s+(.+)$', re.M)
ERRFILE_RE = re.compile(r'^#define\s+(ERRFILE_[^\s]+)\s+(.+)$', re.M)
POSIX_ERRNO_RE = re.compile(r'^#define\s+(E[A-Z0-9]+)\s+(?:\\\n)?.*(0x[0-9a-f]+).*$', re.M)

def err(msg):
    sys.stderr.write('%s: %s\n' % (sys.argv[0], msg))
    sys.exit(1)

def to_pxenv_status(errno):
    return errno & 0xff

def to_errfile(errno):
    return (errno >> 13) & 0x7ff

def to_posix_errno(errno):
    return (errno >> 24) & 0x7f

def load_header_file(filename, regexp):
    defines = {}
    data = open(filename, 'r').read()
    for m in regexp.finditer(data):
        key, val = m.groups()
        defines[key] = val
    return defines

def evaluate(defines, expr):
    pyexpr = ''
    for token in expr.split():
        if token in '()':
            pass
        elif token.startswith('/*') or token.startswith('//'):
            break
        elif token.startswith('0x') or token == '|':
            pyexpr += token
        else:
            if token in defines:
                pyexpr += '0x%x' % defines[token]
            else:
                return -1
    if not re.match(r'^[0-9a-zA-Z_|]+$', pyexpr):
        err('invalid expression')
    return eval(pyexpr)

def build(filenames, regexp, selector):
    unevaluated = {}
    for filename in filenames:
        unevaluated.update(load_header_file(filename, regexp))

    evaluated = {}
    changed = True
    while changed:
        changed = False
        for key in list(unevaluated.keys()):
            val = evaluate(evaluated, unevaluated[key])
            if val != -1:
                del unevaluated[key]
                evaluated[key] = val
                changed = True
    if unevaluated:
        err('unable to evaluate all #defines')

    lookup = {}
    for key, val in evaluated.iteritems():
        lookup[selector(val)] = key
    return lookup

print 'pxenv_status =', repr(build(pxenv_status_files, PXENV_STATUS_RE, to_pxenv_status))
print 'errfile =', repr(build(errfile_files, ERRFILE_RE, to_errfile))
print 'posix_errno =', repr(build(posix_errno_files, POSIX_ERRNO_RE, to_posix_errno))
