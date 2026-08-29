#!/usr/bin/env python3

import argparse
import base64
from enum import Enum
import hashlib
import itertools
import json
from pathlib import Path
import textwrap
from typing import ClassVar

import attrs

##############################################################################
#
# Attribute-based class helpers
#

attrclass = attrs.define(frozen=True, kw_only=True)

def scalar_field(cls, **kwargs):
    """Define an auto-converting field holding a single scalar value"""
    if attrs.has(cls):
        converter = lambda x: cls(**x)
    else:
        converter = cls
    if "default" in kwargs:
        converter = attrs.converters.optional(converter)
    return attrs.field(converter=converter, **kwargs)

def list_field(cls, **kwargs):
    """Define an auto-converting field holding an ordered list of values"""
    if attrs.has(cls):
        converter = lambda l: [cls(**x) for x in l]
    else:
        converter = lambda l: [cls(x) for x in l]
    return attrs.field(converter=converter, **kwargs)

def set_field(cls, **kwargs):
    """Define an auto-converting field holding an unordered set of values"""
    if attrs.has(cls):
        converter = lambda l: {cls(**x) for x in l}
    else:
        converter = lambda l: {cls(x) for x in l}
    return attrs.field(converter=converter, **kwargs)

def map_field(cls, **kwargs):
    """Define an auto-converting field holding a map of values"""
    if attrs.has(cls):
        converter = lambda d: {k: cls(**v) for k, v in d.items()}
    else:
        converter = lambda d: {k: cls(v) for k, v in d.items()}
    return attrs.field(converter=converter, **kwargs)

##############################################################################
#
# Common types
#

class HexBytes(bytes):
    """A hex-encoded byte string"""

    def __new__(cls, value):
        if isinstance(value, str):
            value = base64.b16decode(value, casefold=True)
        return super().__new__(cls, value)

    def __str__(self):
        return base64.b16encode(self).decode().lower()

    def source(self, prefix, size, width=80):
        """Generate source code fragment"""
        pre = prefix.expandtabs() + " ( "
        mid = " " * len(pre)
        post = " )"
        count = (width - len(pre + post)) // len("0x00, ")
        value = self[-size:].rjust(size, b"\x00")
        code = (
            pre +
            (",\n" + mid).join(
                ", ".join("0x%02x" % x for x in batch)
                for batch in itertools.batched(value, count)
            ) +
            post
        ).replace("\t".expandtabs(), "\t")
        return code

class TestResult(Enum):
    """A test result"""
    ACCEPTABLE = "acceptable"
    VALID = "valid"
    INVALID = "invalid"

class TestBugType(Enum):
    """A test bug type"""
    AUTH_BYPASS = "AUTH_BYPASS"
    BASIC = "BASIC"
    BER_ENCODING = "BER_ENCODING"
    CAN_OF_WORMS = "CAN_OF_WORMS"
    CONFIDENTIALITY = "CONFIDENTIALITY"
    DEFINED = "DEFINED"
    EDGE_CASE = "EDGE_CASE"
    FUNCTIONALITY = "FUNCTIONALITY"
    KNOWN_BUG = "KNOWN_BUG"
    LEGACY = "LEGACY"
    MALLEABILITY = "MALLEABILITY"
    MISSING_PARAMETER = "MISSING_PARAMETER"
    MISSING_STEP = "MISSING_STEP"
    MODIFIED_PARAMETER = "MODIFIED_PARAMETER"
    SIGNATURE_MALLEABILITY = "SIGNATURE_MALLEABILITY"
    UNKNOWN = "UNKNOWN"
    WEAK_PARAMS = "WEAK_PARAMS"
    WRONG_PRIMITIVE = "WRONG_PRIMITIVE"

@attrclass
class TestNote:
    """A test note"""
    bugType = scalar_field(TestBugType)
    cves = list_field(str, factory=list)
    description = scalar_field(str, default=None)
    effect = scalar_field(str, default=None)
    links = list_field(str, factory=list)

@attrclass
class TestSource:
    """A test source"""
    name = scalar_field(str)
    version = scalar_field(str)

@attrclass
class TestCase:
    """A test case"""
    ALGORITHM: ClassVar = None
    comment = scalar_field(str)
    flags = set_field(str)
    result = scalar_field(TestResult)
    tcId = scalar_field(int)

    def validate_fixed(self, attr, value, size):
        """Validate a fixed-size attribute"""
        if self.skip:
            return
        if len(value) != size:
            raise ValueError(
                "%s: incorrect size %d (expected %d) for %s" %
                (attr.name, len(value), size, value)
            )

    def validate_padded(self, attr, value, size):
        """Validate a potentially zero-padded attribute"""
        if self.skip:
            return
        if any(x for x in value[:-size]):
            raise ValueError(
                "%s: non-zero leading padding in %s" %
                (attr.name, value)
            )

    @property
    def skip(self):
        """Reason for skipping test (if any)"""
        return None

    @property
    def failure(self):
        """Check if test case is expected to fail"""
        return 0

    @property
    def stable_id(self):
        """Calculate a stable test ID

        The tcId provides a stable ordering but not a stable
        numbering.  To avoid unnecessary churn when tests are
        renumbered upstream, we construct a stable identifier built
        from the relevant parameters of the test itself.
        """
        props = {
            field.name.encode(): str(getattr(self, field.name)).encode()
            for field in attrs.fields(type(self))
            if field.metadata.get("stable")
        }
        digest = hashlib.sha256()
        digest.update(b"algorithm:%s" % self.ALGORITHM.encode())
        for name, value in sorted(props.items()):
            digest.update(b":%s:%d:%s" % (name, len(value), value))
        return digest.hexdigest()[:8]

    @property
    def test_name(self):
        """Test case name"""
        return "wycheproof_%s_%s" % (self.ALGORITHM, self.stable_id)

    def definition(self):
        """Generate source code for test definition

        We include the tcId within the source code comment to enable
        easy traceability of any failures back to the upstream test
        case.
        """
        skip = self.skip
        code = (
            "/* %s test case %d" % (self.ALGORITHM.upper(), self.tcId) +
            (" (skipped: %s)" % skip if skip else "") +
            " */\n"
        )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        return ""

@attrclass
class TestGroup:
    """A test group"""
    source = scalar_field(TestSource)
    tests = list_field(TestCase)
    type = scalar_field(str)

@attrclass
class TestFile:
    """A test file"""
    SCHEMA: ClassVar = None
    SRCFILE: ClassVar = None
    DSTFILE: ClassVar = None
    algorithm = scalar_field(str, default=None)
    header = list_field(str, factory=list)
    notes = map_field(TestNote, factory=dict)
    numberOfTests = scalar_field(int)
    schema = scalar_field(str)
    testGroups = list_field(TestGroup)

    @property
    def tests(self):
        """All test cases"""
        return [test for group in self.testGroups for test in group.tests]

    def source(self):
        """Generate source code"""
        if self.schema != self.SCHEMA:
            raise ValueError(
                "%s: found schema %s (expected %s)" %
                (self.SRCFILE, self.schema, self.SCHEMA)
            )
        generator = Path(__file__).name
        tests = self.tests
        if len(tests) != self.numberOfTests:
            raise ValueError(
                "%s: found %d tests (expected %d)" %
                (self.SRCFILE, len(tests), self.numberOfTests)
            )
        algorithm = tests[0].ALGORITHM
        definitions = "\n".join(x.definition() for x in tests)
        invocations = "".join(x.invocation() for x in tests if not x.skip)
        code = (
            textwrap.dedent(f"""
            /* This file is automatically generated by {generator}.
             *
             * DO NOT EDIT THIS FILE.  Any changes will be lost.
             *
             */

            #include "../wycheproof_test.h"

            """).lstrip() +
            definitions +
            textwrap.dedent(f"""
            /** Perform Wycheproof {algorithm} self-tests */
            void wycheproof_{algorithm}_exec ( void ) {{

            \t/* Perform tests in tcId order */
            """) +
            invocations +
            "}\n"
        )
        return code

    @classmethod
    def load(cls, fh):
        """Load from JSON input file handle"""
        data = json.load(fh)
        return cls(**data)

    @classmethod
    def read(cls, filename):
        """Read from JSON input file"""
        with open(filename, "rt") as fh:
            return cls.load(fh)

    def write(self, filename):
        """Write source output file"""
        with open(filename, "wt") as fh:
            fh.write(self.source())

##############################################################################
#
# Key exchange tests
#

@attrclass
class ExchangeTestCase(TestCase):
    """A key exchange test case"""
    PRIVSIZE: ClassVar = None
    PUBSIZE: ClassVar = None
    SHAREDSIZE: ClassVar = None
    private = scalar_field(HexBytes, metadata={"stable": True})
    public = scalar_field(HexBytes, metadata={"stable": True})
    shared = scalar_field(HexBytes)

    @private.validator
    def validate_private(self, attr, value):
        """Validate private key size"""
        self.validate_padded(attr, value, self.PRIVSIZE)

    @public.validator
    def validate_public(self, attr, value):
        """Validate public key size"""
        self.validate_fixed(attr, value, self.PUBSIZE)

    @shared.validator
    def validate_shared(self, attr, value):
        """Validate shared key size"""
        if not self.failure:
            self.validate_fixed(attr, value, self.SHAREDSIZE)

    @property
    def failure(self):
        return self.result is TestResult.INVALID

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        if not self.skip:
            algorithm = "&%s_algorithm" % self.ALGORITHM
            code += (
                "EXCHANGE_TEST ( %s, %s,\n" % (self.test_name, algorithm) +
                self.private.source("\tPRIVATE", self.PRIVSIZE) + ",\n" +
                self.public.source("\tPARTNER", self.PUBSIZE) + ",\n" +
                "\tPUBLIC_UNSPECIFIED,\n" +
                ("\tSHARED_FAIL" if self.failure else
                 self.shared.source("\tSHARED", self.SHAREDSIZE)) + " );\n"
            )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        code += "\texchange_ok ( &%s );\n" % self.test_name
        return code

##############################################################################
#
# NIST key exchange tests
#

@attrclass
class NistExchangeTestCase(ExchangeTestCase):
    """A NIST elliptic curve key exchange test case"""

    @property
    def skip(self):
        if not self.public:
            return "no public key"
        if self.public[0] in (0x02, 0x03):
            return "compressed"

##############################################################################
#
# P-256 key exchange tests
#

@attrclass
class P256ExchangeTestCase(NistExchangeTestCase):
    """A P-256 key exchange test case"""
    ALGORITHM: ClassVar = "p256"
    PRIVSIZE: ClassVar = 32
    PUBSIZE: ClassVar = 65
    SHAREDSIZE: ClassVar = 32

@attrclass
class P256ExchangeTestGroup(TestGroup):
    """A P-256 key exchange test group"""
    curve = scalar_field(str)
    encoding = scalar_field(str)
    tests = list_field(P256ExchangeTestCase)

@attrclass
class P256ExchangeTestFile(TestFile):
    """A P-256 key exchange test file"""
    SCHEMA: ClassVar = "ecdh_ecpoint_test_schema_v1.json"
    SRCFILE: ClassVar = "ecdh_secp256r1_ecpoint_test.json"
    DSTFILE: ClassVar = "wycheproof_p256.c"
    testGroups = list_field(P256ExchangeTestGroup)

##############################################################################
#
# P-384 key exchange tests
#

@attrclass
class P384ExchangeTestCase(NistExchangeTestCase):
    """A P-384 key exchange test case"""
    ALGORITHM: ClassVar = "p384"
    PRIVSIZE: ClassVar = 48
    PUBSIZE: ClassVar = 97
    SHAREDSIZE: ClassVar = 48

@attrclass
class P384ExchangeTestGroup(TestGroup):
    """A P-384 key exchange test group"""
    curve = scalar_field(str)
    encoding = scalar_field(str)
    tests = list_field(P384ExchangeTestCase)

@attrclass
class P384ExchangeTestFile(TestFile):
    """A P-384 key exchange test file"""
    SCHEMA: ClassVar = "ecdh_ecpoint_test_schema_v1.json"
    SRCFILE: ClassVar = "ecdh_secp384r1_ecpoint_test.json"
    DSTFILE: ClassVar = "wycheproof_p384.c"
    testGroups = list_field(P384ExchangeTestGroup)

##############################################################################
#
# X25519 key exchange tests
#

class X25519TestFlag(Enum):
    """An X25519 test flag"""
    EDGE_CASE_MULTIPLICATION = "EdgeCaseMultiplication"
    EDGE_CASE_PRIVATE_KEY = "EdgeCasePrivateKey"
    EDGE_CASE_SHARED = "EdgeCaseShared"
    KTV = "Ktv"
    LOW_ORDER_PUBLIC = "LowOrderPublic"
    NON_CANONICAL_PUBLIC = "NonCanonicalPublic"
    NORMAL = "Normal"
    SMALL_PUBLIC_KEY = "SmallPublicKey"
    SPECIAL_PUBLIC_KEY = "SpecialPublicKey"
    TWIST = "Twist"
    ZERO_SHARED_SECRET = "ZeroSharedSecret"

@attrclass
class X25519TestCase(ExchangeTestCase):
    """An X25519 key exchange test case"""
    ALGORITHM: ClassVar = "x25519"
    PRIVSIZE: ClassVar = 32
    PUBSIZE: ClassVar = 32
    SHAREDSIZE: ClassVar = 32
    flags = set_field(X25519TestFlag)

    @property
    def failure(self):
        return X25519TestFlag.ZERO_SHARED_SECRET in self.flags

@attrclass
class X25519TestGroup(TestGroup):
    """An X25519 key exchange test group"""
    curve = scalar_field(str)
    tests = list_field(X25519TestCase)

@attrclass
class X25519TestFile(TestFile):
    """An X25519 key exchange test file"""
    SCHEMA: ClassVar = "xdh_comp_schema_v1.json"
    SRCFILE: ClassVar = "x25519_test.json"
    DSTFILE: ClassVar = "wycheproof_x25519.c"
    testGroups = list_field(X25519TestGroup)

##############################################################################
#
# Main program
#

def main():
    """Main program"""

    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        description="Import Project Wycheproof test cases"
    )
    parser.add_argument("dir", help="Project Wycheproof repository checkout")
    args = parser.parse_args()

    # Locate Project Wycheproof source files
    srcdir = Path(args.dir) / "testvectors_v1"
    if not srcdir.exists():
        raise FileNotFoundError(srcdir)

    # Locate iPXE output directory
    dstdir = Path(__file__).parent.parent / "wycheproof"
    if not dstdir.exists():
        raise FileNotFoundError(dstdir)

    # Read JSON inputs
    classes = (
        P256ExchangeTestFile,
        P384ExchangeTestFile,
        X25519TestFile,
    )
    tests = [x.read(srcdir / x.SRCFILE) for x in classes]

    # Write source code outputs
    for test in tests:
        test.write(dstdir / test.DSTFILE)

if __name__ == "__main__":
    main()
