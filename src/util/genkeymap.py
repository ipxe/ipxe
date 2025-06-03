#!/usr/bin/env python3
#
# Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

"""Generate iPXE keymaps"""

from __future__ import annotations

import argparse
from collections import UserDict
from collections.abc import Sequence, Mapping, MutableMapping
from dataclasses import dataclass
from enum import Flag, IntEnum
import re
import subprocess
from struct import Struct
import textwrap
from typing import ClassVar, Optional


BACKSPACE = chr(0x7f)
"""Backspace character"""


class KeyType(IntEnum):
    """Key types"""

    LATIN = 0
    FN = 1
    SPEC = 2
    PAD = 3
    DEAD = 4
    CONS = 5
    CUR = 6
    SHIFT = 7
    META = 8
    ASCII = 9
    LOCK = 10
    LETTER = 11
    SLOCK = 12
    DEAD2 = 13
    BRL = 14
    UNKNOWN = 0xf0


class DeadKey(IntEnum):
    """Dead keys"""

    GRAVE = 0
    CIRCUMFLEX = 2
    TILDE = 3


class KeyModifiers(Flag):
    """Key modifiers"""

    NONE = 0
    SHIFT = 1
    ALTGR = 2
    CTRL = 4
    ALT = 8
    SHIFTL = 16
    SHIFTR = 32
    CTRLL = 64
    CTRLR = 128

    @property
    def complexity(self) -> int:
        """Get complexity value of applied modifiers"""
        if self == self.NONE:
            return 0
        if self == self.SHIFT:
            return 1
        if self == self.CTRL:
            return 2
        return 3 + bin(self.value).count('1')


@dataclass(frozen=True)
class Key:
    """A single key definition"""

    keycode: int
    """Opaque keycode"""

    keysym: int
    """Key symbol"""

    modifiers: KeyModifiers
    """Applied modifiers"""

    ASCII_TYPES: ClassVar[set[KeyType]] = {KeyType.LATIN, KeyType.ASCII,
                                           KeyType.LETTER}
    """Key types with direct ASCII values"""

    DEAD_KEYS: ClassVar[Mapping[int, str]] = {
        DeadKey.GRAVE: '`',
        DeadKey.CIRCUMFLEX: '^',
        DeadKey.TILDE: '~',
    }
    """Dead key replacement ASCII values"""

    @property
    def keytype(self) -> Optional[KeyType]:
        """Key type"""
        try:
            return KeyType(self.keysym >> 8)
        except ValueError:
            return None

    @property
    def value(self) -> int:
        """Key value"""
        return self.keysym & 0xff

    @property
    def ascii(self) -> Optional[str]:
        """ASCII character"""
        keytype = self.keytype
        value = self.value
        if keytype in self.ASCII_TYPES:
            char = chr(value)
            if value and char.isascii():
                return char
        if keytype == KeyType.DEAD:
            return self.DEAD_KEYS.get(value)
        return None


class KeyLayout(UserDict[KeyModifiers, Sequence[Key]]):
    """A keyboard layout"""

    BKEYMAP_MAGIC: ClassVar[bytes] = b'bkeymap'
    """Magic signature for output produced by 'loadkeys -b'"""

    MAX_NR_KEYMAPS: ClassVar[int] = 256
    """Maximum number of keymaps produced by 'loadkeys -b'"""

    NR_KEYS: ClassVar[int] = 128
    """Number of keys in each keymap produced by 'loadkeys -b'"""

    KEY_BACKSPACE: ClassVar[int] = 14
    """Key code for backspace

    Keyboard maps seem to somewhat arbitrarily pick an interpretation
    for the backspace key and its various modifiers, according to the
    personal preference of the keyboard map transcriber.
    """

    KEY_NON_US: ClassVar[int] = 86
    """Key code 86

    Key code 86 is somewhat bizarre.  It doesn't physically exist on
    most US keyboards.  The database used by "loadkeys" defines it as
    "<>", while most other databases either define it as a duplicate
    "\\|" or omit it entirely.
    """

    FIXUPS: ClassVar[Mapping[str, Mapping[KeyModifiers,
                                          Sequence[tuple[int, int]]]]] = {
        'us': {
            # Redefine erroneous key 86 as generating "\\|"
            KeyModifiers.NONE: [(KEY_NON_US, ord('\\'))],
            KeyModifiers.SHIFT: [(KEY_NON_US, ord('|'))],
            # Treat Ctrl-Backspace as producing Backspace rather than Ctrl-H
            KeyModifiers.CTRL: [(KEY_BACKSPACE, ord(BACKSPACE))],
        },
        'il': {
            # Redefine some otherwise unreachable ASCII characters
            # using the closest available approximation
            KeyModifiers.ALTGR: [(0x28, ord('\'')), (0x2b, ord('`')),
                                 (0x35, ord('/'))],
        },
        'mt': {
            # Redefine erroneous key 86 as generating "\\|"
            KeyModifiers.NONE: [(KEY_NON_US, ord('\\'))],
            KeyModifiers.SHIFT: [(KEY_NON_US, ord('|'))],
        },
    }
    """Fixups for erroneous keymappings produced by 'loadkeys -b'"""

    @property
    def unshifted(self):
        """Basic unshifted keyboard layout"""
        return self[KeyModifiers.NONE]

    @property
    def shifted(self):
        """Basic shifted keyboard layout"""
        return self[KeyModifiers.SHIFT]

    @classmethod
    def load(cls, name: str) -> KeyLayout:
        """Load keymap using 'loadkeys -b'"""
        bkeymap = subprocess.check_output(["loadkeys", "-u", "-b", name])
        if not bkeymap.startswith(cls.BKEYMAP_MAGIC):
            raise ValueError("Invalid bkeymap magic signature")
        bkeymap = bkeymap[len(cls.BKEYMAP_MAGIC):]
        included = bkeymap[:cls.MAX_NR_KEYMAPS]
        if len(included) != cls.MAX_NR_KEYMAPS:
            raise ValueError("Invalid bkeymap inclusion list")
        bkeymap = bkeymap[cls.MAX_NR_KEYMAPS:]
        keys = {}
        for modifiers in map(KeyModifiers, range(cls.MAX_NR_KEYMAPS)):
            if included[modifiers.value]:
                fmt = Struct('<%dH' % cls.NR_KEYS)
                bkeylist = bkeymap[:fmt.size]
                if len(bkeylist) != fmt.size:
                    raise ValueError("Invalid bkeymap map %#x" %
                                     modifiers.value)
                keys[modifiers] = [
                    Key(modifiers=modifiers, keycode=keycode, keysym=keysym)
                    for keycode, keysym in enumerate(fmt.unpack(bkeylist))
                ]
                bkeymap = bkeymap[len(bkeylist):]
        if bkeymap:
            raise ValueError("Trailing bkeymap data")
        for modifiers, fixups in cls.FIXUPS.get(name, {}).items():
            for keycode, keysym in fixups:
                keys[modifiers][keycode] = Key(modifiers=modifiers,
                                               keycode=keycode, keysym=keysym)
        return cls(keys)

    @property
    def inverse(self) -> MutableMapping[str, Key]:
        """Construct inverse mapping from ASCII value to key"""
        return {
            key.ascii: key
            # Give priority to simplest modifier for a given ASCII code
            for modifiers in sorted(self.keys(), reverse=True,
                                    key=lambda x: (x.complexity, x.value))
            # Give priority to lowest keycode for a given ASCII code
            for key in reversed(self[modifiers])
            # Ignore keys with no ASCII value
            if key.ascii
        }


class BiosKeyLayout(KeyLayout):
    """Keyboard layout as used by the BIOS

    To allow for remappings of the somewhat interesting key 86, we
    arrange for our keyboard drivers to generate this key as "\\|"
    with the high bit set.
    """

    KEY_PSEUDO: ClassVar[int] = 0x80
    """Flag used to indicate a fake ASCII value"""

    KEY_NON_US_UNSHIFTED: ClassVar[str] = chr(KEY_PSEUDO | ord('\\'))
    """Fake ASCII value generated for unshifted key code 86"""

    KEY_NON_US_SHIFTED: ClassVar[str] = chr(KEY_PSEUDO | ord('|'))
    """Fake ASCII value generated for shifted key code 86"""

    @property
    def inverse(self) -> MutableMapping[str, Key]:
        inverse = super().inverse
        assert len(inverse) == 0x7f
        inverse[self.KEY_NON_US_UNSHIFTED] = self.unshifted[self.KEY_NON_US]
        inverse[self.KEY_NON_US_SHIFTED] = self.shifted[self.KEY_NON_US]
        assert all(x.modifiers in {KeyModifiers.NONE, KeyModifiers.SHIFT,
                                   KeyModifiers.CTRL}
                   for x in inverse.values())
        return inverse


class KeymapKeys(UserDict[str, Optional[str]]):
    """An ASCII character remapping"""

    @classmethod
    def ascii_name(cls, char: str) -> str:
        """ASCII character name"""
        if char == '\\':
            name = "'\\\\'"
        elif char == '\'':
            name = "'\\\''"
        elif ord(char) & BiosKeyLayout.KEY_PSEUDO:
            name = "Pseudo-%s" % cls.ascii_name(
                chr(ord(char) & ~BiosKeyLayout.KEY_PSEUDO)
            )
        elif char.isprintable():
            name = "'%s'" % char
        elif ord(char) <= 0x1a:
            name = "Ctrl-%c" % (ord(char) + 0x40)
        else:
            name = "0x%02x" % ord(char)
        return name

    @property
    def code(self):
        """Generated source code for C array"""
        return '{\n' + ''.join(
            '\t{ 0x%02x, 0x%02x },\t/* %s => %s */\n' % (
                ord(source), ord(target),
                self.ascii_name(source), self.ascii_name(target)
            )
            for source, target in self.items()
            if target
            and ord(source) & ~BiosKeyLayout.KEY_PSEUDO != ord(target)
        ) + '\t{ 0, 0 }\n}'


@dataclass
class Keymap:
    """An iPXE keyboard mapping"""

    name: str
    """Mapping name"""

    source: KeyLayout
    """Source keyboard layout"""

    target: KeyLayout
    """Target keyboard layout"""

    @property
    def basic(self) -> KeymapKeys:
        """Basic remapping table"""
        # Construct raw mapping from source ASCII to target ASCII
        raw = {source: self.target[key.modifiers][key.keycode].ascii
               for source, key in self.source.inverse.items()}
        # Eliminate any identity mappings, or mappings that attempt to
        # remap the backspace key
        table = {source: target for source, target in raw.items()
                 if source != target
                 and source != BACKSPACE
                 and target != BACKSPACE}
        # Recursively delete any mappings that would produce
        # unreachable alphanumerics (e.g. the "il" keymap, which maps
        # away the whole lower-case alphabet)
        while True:
            unreachable = set(table.keys()) - set(table.values())
            delete = {x for x in unreachable if x.isascii() and x.isalnum()}
            if not delete:
                break
            table = {k: v for k, v in table.items() if k not in delete}
        # Sanity check: ensure that all numerics are reachable using
        # the same shift state
        digits = '1234567890'
        unshifted = ''.join(table.get(x) or x for x in '1234567890')
        shifted = ''.join(table.get(x) or x for x in '!@#$%^&*()')
        if digits not in (shifted, unshifted):
            raise ValueError("Inconsistent numeric remapping %s / %s" %
                             (unshifted, shifted))
        return KeymapKeys(dict(sorted(table.items())))

    @property
    def altgr(self) -> KeymapKeys:
        """AltGr remapping table"""
        # Construct raw mapping from source ASCII to target ASCII
        raw = {
            source: next((self.target[x][key.keycode].ascii
                          for x in (key.modifiers | KeyModifiers.ALTGR,
                                    KeyModifiers.ALTGR, key.modifiers)
                          if x in self.target
                          and self.target[x][key.keycode].ascii), None)
            for source, key in self.source.inverse.items()
        }
        # Identify printable keys that are unreachable via the basic map
        basic = self.basic
        unmapped = set(x for x in basic.keys()
                       if x.isascii() and x.isprintable())
        remapped = set(basic.values())
        unreachable = unmapped - remapped
        # Eliminate any mappings for unprintable characters, or
        # mappings for characters that are reachable via the basic map
        table = {source: target for source, target in raw.items()
                 if source.isprintable()
                 and target in unreachable}
        # Check that all characters are now reachable
        unreachable -= set(table.values())
        if unreachable:
            raise ValueError("Unreachable characters: %s" % ', '.join(
                KeymapKeys.ascii_name(x) for x in sorted(unreachable)
            ))
        return KeymapKeys(dict(sorted(table.items())))

    def cname(self, suffix: str) -> str:
        """C variable name"""
        return re.sub(r'\W', '_', (self.name + '_' + suffix))

    @property
    def code(self) -> str:
        """Generated source code"""
        keymap_name = self.cname("keymap")
        basic_name = self.cname("basic")
        altgr_name = self.cname("altgr")
        attribute = "__keymap_default" if self.name == "us" else "__keymap"
        code = textwrap.dedent(f"""
        /** @file
         *
         * "{self.name}" keyboard mapping
         *
         * This file is automatically generated; do not edit
         *
         */

        FILE_LICENCE ( PUBLIC_DOMAIN );

        #include <ipxe/keymap.h>

        /** "{self.name}" basic remapping */
        static struct keymap_key {basic_name}[] = %s;

        /** "{self.name}" AltGr remapping */
        static struct keymap_key {altgr_name}[] = %s;

        /** "{self.name}" keyboard map */
        struct keymap {keymap_name} {attribute} = {{
        \t.name = "{self.name}",
        \t.basic = {basic_name},
        \t.altgr = {altgr_name},
        }};
        """).strip() % (self.basic.code, self.altgr.code)
        return code


if __name__ == '__main__':

    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Generate iPXE keymaps")
    parser.add_argument('--verbose', '-v', action='count', default=0,
                        help="Increase verbosity")
    parser.add_argument('layout', help="Target keyboard layout")
    args = parser.parse_args()

    # Load source and target keyboard layouts
    source = BiosKeyLayout.load('us')
    target = KeyLayout.load(args.layout)

    # Construct keyboard mapping
    keymap = Keymap(name=args.layout, source=source, target=target)

    # Output generated code
    print(keymap.code)
