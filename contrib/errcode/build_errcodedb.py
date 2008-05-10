#!/usr/bin/env python
import sys
import re

pxenv_status_files = ('../../src/include/errno.h', )
errfile_files = ('../../src/include/gpxe/errfile.h',
            '../../src/arch/i386/include/bits/errfile.h')
posix_errno_files = ('../../src/include/errno.h', )

PXENV_STATUS_RE = re.compile(r'^#define\s+(PXENV_STATUS_[^\s]+)\s+(.+)$')
ERRFILE_RE = re.compile(r'^#define\s+(ERRFILE_[^\s]+)\s+(.+)$')
POSIX_ERRNO_RE = re.compile(r'^#define\s+(E[A-Z]+)\s+.*(0x[0-9a-f]+).*$')

def err(msg):
    sys.stderr.write('%s: %s\n' % (sys.argv[0], msg))
    sys.exit(1)

def load_header_file(filename, regexp):
    defines = {}
    for line in open(filename, 'r'):
        m = regexp.match(line)
        if m:
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

def build(filenames, regexp):
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
    return evaluated

print 'pxenv_status =', repr(build(pxenv_status_files, PXENV_STATUS_RE))
print 'errfile =', repr(build(errfile_files, ERRFILE_RE))
print 'posix_errno =', repr(build(posix_errno_files, POSIX_ERRNO_RE))
