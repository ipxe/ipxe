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
import re
import socket
import errcode

HOST = 'irc.freenode.net'
PORT = 6667
NICK = 'gpxebot'
CHAN = '#etherboot'
NICKSERV_PASSWORD = None
IDENT = 'gpxebot'
REALNAME = 'gPXE bot'

ERRCODE_RE = re.compile(r'(errcode|Error)\s+((0x)?[0-9a-fA-F]{8})')

NO_ARGS = -1

handlers = {}

def nick_from_mask(mask):
    return (mask.find('!') > -1 and mask.split('!', 1)[0]) or mask

def autojoin():
    del handlers['376']
    if NICKSERV_PASSWORD:
        pmsg('nickserv', 'identify %s' % NICKSERV_PASSWORD)
    if CHAN:
        cmd('JOIN %s' % CHAN)

def ping(_, arg):
    cmd('PONG %s' % arg)

def privmsg(_, target, msg):
    if target == CHAN:
        replyto = target
        if msg.find(NICK) == -1:
            return
    elif target == NICK:
        replyto = nick_from_mask(who)
    m = ERRCODE_RE.search(msg)
    if m:
        try:
            pmsg(replyto, str(errcode.Errcode(int(m.groups()[1], 16))))
        except ValueError:
            pass
    if msg.find('help') > -1:
        pmsg(replyto, 'I look up gPXE error codes.  Message me like this:')
        pmsg(replyto, 'errcode 0x12345678  OR  Error 0x12345678')

def add_handler(command, handler, nargs):
    handlers[command] = (handler, nargs)

def cmd(msg):
    sock.sendall('%s\r\n' % msg)

def pmsg(target, msg):
    cmd('PRIVMSG %s :%s' % (target, msg))

def dispatch(args):
    command = args[0]
    if command in handlers:
        h = handlers[command]
        if h[1] == NO_ARGS:
            h[0]()
        elif len(args) == h[1]:
            h[0](*args)

def parse(line):
    if line[0] == ':':
        who, line = line.split(None, 1)
        who = who[1:]
    else:
        who = None
    args = []
    while line and line[0] != ':' and line.find(' ') != -1:
        fields = line.split(None, 1)
        if len(fields) == 1:
            fields.append(None)
        arg, line = fields
        args.append(arg)
    if line:
        if line[0] == ':':
            args.append(line[1:])
        else:
            args.append(line)
    return who, args

add_handler('376', autojoin, NO_ARGS)
add_handler('PING', ping, 2)
add_handler('PRIVMSG', privmsg, 3)

sock = socket.socket()
sock.connect((HOST, PORT))
cmd('NICK %s' % NICK)
cmd('USER %s none none :%s' % (IDENT, REALNAME))

rbuf = ''
while True:
    r = sock.recv(4096)
    if not r:
        break
    rbuf += r

    while rbuf.find('\r\n') != -1:
        line, rbuf = rbuf.split('\r\n', 1)
        if not line:
            continue
        who, args = parse(line)
        dispatch(args)
