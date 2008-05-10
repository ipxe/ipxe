#!/usr/bin/env python
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

def lookup_errno_component(defines, selector, component):
    for key, val in defines.iteritems():
        if selector(val) == component:
            return key
    return '0x%x' % component

class Errno(object):
    def __init__(self, errno):
        self.pxenv_status = to_pxenv_status(errno)
        self.uniq = to_uniq(errno)
        self.errfile = to_errfile(errno)
        self.posix_errno = to_posix_errno(errno)

    def rawstr(self):
        return 'pxenv_status=0x%x uniq=%d errfile=0x%x posix_errno=0x%x' % (self.pxenv_status, self.uniq, self.errfile, self.posix_errno)

    def prettystr(self):
        return 'pxenv_status=%s uniq=%d errfile=%s posix_errno=%s' % (
                lookup_errno_component(errcodedb.pxenv_status, to_pxenv_status, self.pxenv_status),
                self.uniq,
                lookup_errno_component(errcodedb.errfile, to_errfile, self.errfile),
                lookup_errno_component(errcodedb.posix_errno, to_posix_errno, self.posix_errno)
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

    print Errno(errno)
    sys.exit(0)
