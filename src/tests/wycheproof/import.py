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
import weakref

import attrs

##############################################################################
#
# Attribute-based class helpers
#

attrclass = attrs.define(frozen=True, kw_only=True)

def auto_converter(typ):
    """Define an automatic converter"""
    if attrs.has(typ):
        has_parent = ("_parent" in attrs.fields_dict(typ))
        def converter(value, self):
            if has_parent:
                value["_parent"] = self
            return typ(**value)
    else:
        def converter(value, self):
            return typ(value)
    return converter

def scalar_field(typ, **kwargs):
    """Define an auto-converting field holding a single scalar value"""
    converter = attrs.Converter(auto_converter(typ), takes_self=True)
    if "default" in kwargs:
        converter = attrs.converters.optional(converter)
    return attrs.field(converter=converter, **kwargs)

def list_field(typ, **kwargs):
    """Define an auto-converting field holding an ordered list of values"""
    subconverter = auto_converter(typ)
    converter = attrs.Converter(
        lambda l, self: [subconverter(x, self) for x in l],
        takes_self=True,
    )
    return attrs.field(converter=converter, **kwargs)

def set_field(typ, **kwargs):
    """Define an auto-converting field holding an unordered set of values"""
    subconverter = auto_converter(typ)
    converter = attrs.Converter(
        lambda l, self: {subconverter(x, self) for x in l},
        takes_self=True,
    )
    return attrs.field(converter=converter, **kwargs)

def map_field(typ, **kwargs):
    """Define an auto-converting field holding a map of values"""
    subconverter = auto_converter(typ)
    converter = attrs.Converter(
        lambda d, self: {k: subconverter(v, self) for k, v in d.items()},
        takes_self=True,
    )
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

    def source(self, prefix, size=0, width=80):
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
        ).replace("\t".expandtabs(), "\t").replace(" (  )", "()")
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
    _parent = scalar_field(weakref.proxy, alias="_parent")
    comment = scalar_field(str)
    flags = set_field(str)
    result = scalar_field(TestResult)
    tcId = scalar_field(int)

    @property
    def test_group(self):
        """Containing test group"""
        return self._parent

    @property
    def test_file(self):
        """Containing test file"""
        return self.test_group.test_file

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
        return self.result is TestResult.INVALID

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
        digest.update(b"algorithm:%s" % self.test_file.ALGORITHM.encode())
        for name, value in sorted(props.items()):
            digest.update(b":%s:%d:%s" % (name, len(value), value))
        return digest.hexdigest()[:8]

    @property
    def test_name(self):
        """Test case name"""
        return "wycheproof_%s_%s" % (self.test_file.basename, self.stable_id)

    @property
    def test_label(self):
        """Test case label"""
        return self.test_file.LABEL

    def definition(self):
        """Generate source code for test definition

        We include the tcId within the source code comment to enable
        easy traceability of any failures back to the upstream test
        case.
        """
        skip = self.skip
        code = (
            "/* %s test case %d" % (self.test_label, self.tcId) +
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
    _parent = scalar_field(weakref.proxy, alias="_parent")
    source = scalar_field(TestSource)
    tests = list_field(TestCase)
    type = scalar_field(str)

    @property
    def test_file(self):
        """Containing test file"""
        return self._parent

@attrclass
class TestFile:
    """A test file"""
    ALGORITHM: ClassVar = None
    LABEL: ClassVar = None
    SCHEMA: ClassVar = None
    SRCFILE: ClassVar = None
    algorithm = scalar_field(str, default=None)
    header = list_field(str, factory=list)
    notes = map_field(TestNote, factory=dict)
    numberOfTests = scalar_field(int)
    schema = scalar_field(str)
    testGroups = list_field(TestGroup)

    @property
    def basename(self):
        """Base name for test cases"""
        return self.ALGORITHM

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
        execname = "wycheproof_%s_exec" % self.basename
        tests = self.tests
        label = tests[0].test_label
        if len(tests) != self.numberOfTests:
            raise ValueError(
                "%s: found %d tests (expected %d)" %
                (self.SRCFILE, len(tests), self.numberOfTests)
            )
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
            /** Perform Wycheproof {label} self-tests */
            void {execname} ( void ) {{

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
    private = scalar_field(HexBytes, metadata={"stable": True})
    public = scalar_field(HexBytes, metadata={"stable": True})
    shared = scalar_field(HexBytes)

    @private.validator
    def validate_private(self, attr, value):
        """Validate private key size"""
        self.validate_padded(attr, value, self.test_file.PRIVSIZE)

    @public.validator
    def validate_public(self, attr, value):
        """Validate public key size"""
        self.validate_fixed(attr, value, self.test_file.PUBSIZE)

    @shared.validator
    def validate_shared(self, attr, value):
        """Validate shared key size"""
        if not self.failure:
            self.validate_fixed(attr, value, self.test_file.SHAREDSIZE)

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        if not self.skip:
            algorithm = "&%s_algorithm" % self.test_file.ALGORITHM
            privsize = self.test_file.PRIVSIZE
            pubsize = self.test_file.PUBSIZE
            sharedsize = self.test_file.SHAREDSIZE
            code += (
                "EXCHANGE_TEST ( %s, %s,\n" % (self.test_name, algorithm) +
                self.private.source("\tPRIVATE", privsize) + ",\n" +
                self.public.source("\tPARTNER", pubsize) + ",\n" +
                "\tPUBLIC_UNSPECIFIED,\n" +
                ("\tSHARED_FAIL" if self.failure else
                 self.shared.source("\tSHARED", sharedsize)) + " );\n"
            )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        code += "\texchange_ok ( &%s );\n" % self.test_name
        return code

@attrclass
class ExchangeTestGroup(TestGroup):
    """A key exchange test group"""
    tests = list_field(ExchangeTestCase)

@attrclass
class ExchangeTestFile(TestFile):
    """A key exchange test file"""
    PRIVSIZE: ClassVar = None
    PUBSIZE: ClassVar = None
    SHAREDSIZE: ClassVar = None
    testGroups = list_field(ExchangeTestGroup)

##############################################################################
#
# NIST key exchange tests
#

@attrclass
class NistExchangeTestCase(ExchangeTestCase):
    """A NIST elliptic curve key exchange test case"""

    @property
    def skip(self):
        """Reason for skipping test (if any)"""
        if not self.public:
            return "no public key"
        if self.public[0] in (0x02, 0x03):
            return "compressed"

@attrclass
class NistExchangeTestGroup(ExchangeTestGroup):
    """A NIST elliptic curve key exchange test group"""
    curve = scalar_field(str)
    encoding = scalar_field(str)
    tests = list_field(NistExchangeTestCase)

@attrclass
class NistExchangeTestFile(ExchangeTestFile):
    """A NIST elliptic curve key exchange test file"""
    SCHEMA: ClassVar = "ecdh_ecpoint_test_schema_v1.json"
    testGroups = list_field(NistExchangeTestGroup)

@attrclass
class P256ExchangeTestFile(NistExchangeTestFile):
    """A P-256 key exchange test file"""
    ALGORITHM: ClassVar = "p256"
    LABEL: ClassVar = "P256"
    SRCFILE: ClassVar = "ecdh_secp256r1_ecpoint_test.json"
    PRIVSIZE: ClassVar = 32
    PUBSIZE: ClassVar = 65
    SHAREDSIZE: ClassVar = 32

@attrclass
class P384ExchangeTestFile(NistExchangeTestFile):
    """A P-384 key exchange test file"""
    ALGORITHM: ClassVar = "p384"
    LABEL: ClassVar = "P384"
    SRCFILE: ClassVar = "ecdh_secp384r1_ecpoint_test.json"
    PRIVSIZE: ClassVar = 48
    PUBSIZE: ClassVar = 97
    SHAREDSIZE: ClassVar = 48

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
    flags = set_field(X25519TestFlag)

    @property
    def failure(self):
        """Check if test case is expected to fail"""
        return X25519TestFlag.ZERO_SHARED_SECRET in self.flags

@attrclass
class X25519TestGroup(ExchangeTestGroup):
    """An X25519 key exchange test group"""
    curve = scalar_field(str)
    tests = list_field(X25519TestCase)

@attrclass
class X25519TestFile(ExchangeTestFile):
    """An X25519 key exchange test file"""
    ALGORITHM: ClassVar = "x25519"
    LABEL: ClassVar = "X25519"
    SCHEMA: ClassVar = "xdh_comp_schema_v1.json"
    SRCFILE: ClassVar = "x25519_test.json"
    PRIVSIZE: ClassVar = 32
    PUBSIZE: ClassVar = 32
    SHAREDSIZE: ClassVar = 32
    testGroups = list_field(X25519TestGroup)

##############################################################################
#
# HMAC tests
#

class HmacTestFlag(Enum):
    """An HMAC test flag"""
    MODIFIED_TAG = "ModifiedTag"
    PSEUDORANDOM = "Pseudorandom"
    TRUNCATED_HMAC = "TruncatedHmac"

@attrclass
class HmacTestCase(TestCase):
    """An HMAC test case"""
    flags = set_field(HmacTestFlag)
    key = scalar_field(HexBytes, metadata={"stable": True})
    msg = scalar_field(HexBytes, metadata={"stable": True})
    tag = scalar_field(HexBytes)

    @key.validator
    def validate_key(self, attr, value):
        """Validate key size"""
        self.validate_fixed(attr, value, (self.test_group.keySize // 8))

    @tag.validator
    def validate_tag(self, attr, value):
        """Validate tag size"""
        self.validate_fixed(attr, value, (self.test_group.tagSize // 8))

    @property
    def skip(self):
        """Reason for skipping test (if any)"""
        if HmacTestFlag.MODIFIED_TAG in self.flags:
            # Our HMAC abstraction covers only generating the digest,
            # not comparing the output to check for a match
            return "modified tag"
        if self.failure:
            # The test suite includes other failures such as using the
            # wrong algorithm, which is not a meaningful test
            return self.comment

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        if not self.skip:
            algorithm = "&%s_algorithm" % self.test_file.ALGORITHM
            code += (
                "HMAC_TEST ( %s, %s,\n" % (self.test_name, algorithm) +
                self.key.source("\tKEY") + ",\n" +
                self.msg.source("\tDATA") + ",\n" +
                self.tag.source("\tEXPECTED") + " );\n"
            )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        code += "\thmac_ok ( &%s );\n" % self.test_name
        return code

@attrclass
class HmacTestGroup(TestGroup):
    """An HMAC test group"""
    keySize = scalar_field(int)
    tagSize = scalar_field(int)
    tests = list_field(HmacTestCase)

@attrclass
class HmacTestFile(TestFile):
    """An HMAC test file"""
    SCHEMA: ClassVar = "mac_test_schema_v1.json"
    testGroups = list_field(HmacTestGroup)

    @property
    def basename(self):
        """Base name for test cases"""
        return "hmac_%s" % self.ALGORITHM

@attrclass
class HmacSha1TestFile(HmacTestFile):
    """An HMAC-SHA1 test file"""
    ALGORITHM: ClassVar = "sha1"
    LABEL: ClassVar = "HMAC-SHA1"
    SRCFILE: ClassVar = "hmac_sha1_test.json"

@attrclass
class HmacSha224TestFile(HmacTestFile):
    """An HMAC-SHA224 test file"""
    ALGORITHM: ClassVar = "sha224"
    LABEL: ClassVar = "HMAC-SHA224"
    SRCFILE: ClassVar = "hmac_sha224_test.json"

@attrclass
class HmacSha256TestFile(HmacTestFile):
    """An HMAC-SHA256 test file"""
    ALGORITHM: ClassVar = "sha256"
    LABEL: ClassVar = "HMAC-SHA256"
    SRCFILE: ClassVar = "hmac_sha256_test.json"

@attrclass
class HmacSha384TestFile(HmacTestFile):
    """An HMAC-SHA384 test file"""
    ALGORITHM: ClassVar = "sha384"
    LABEL: ClassVar = "HMAC-SHA384"
    SRCFILE: ClassVar = "hmac_sha384_test.json"

@attrclass
class HmacSha512TestFile(HmacTestFile):
    """An HMAC-SHA512 test file"""
    ALGORITHM: ClassVar = "sha512"
    LABEL: ClassVar = "HMAC-SHA512"
    SRCFILE: ClassVar = "hmac_sha512_test.json"

@attrclass
class HmacSha512224TestFile(HmacTestFile):
    """An HMAC-SHA512/224 test file"""
    ALGORITHM: ClassVar = "sha512_224"
    LABEL: ClassVar = "HMAC-SHA512/224"
    SRCFILE: ClassVar = "hmac_sha512_224_test.json"

@attrclass
class HmacSha512256TestFile(HmacTestFile):
    """An HMAC-SHA512/256 test file"""
    ALGORITHM: ClassVar = "sha512_256"
    LABEL: ClassVar = "HMAC-SHA512/256"
    SRCFILE: ClassVar = "hmac_sha512_256_test.json"

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
        HmacSha1TestFile,
        HmacSha224TestFile,
        HmacSha256TestFile,
        HmacSha384TestFile,
        HmacSha512TestFile,
        HmacSha512224TestFile,
        HmacSha512256TestFile,
        P256ExchangeTestFile,
        P384ExchangeTestFile,
        X25519TestFile,
    )
    tests = [x.read(srcdir / x.SRCFILE) for x in classes]

    # Write source code outputs
    for test in tests:
        test.write(dstdir / ("wycheproof_%s.c" % test.basename))

if __name__ == "__main__":
    main()
