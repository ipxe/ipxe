#!/usr/bin/env python3

from abc import abstractmethod
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

def map_field(typ, key=str, **kwargs):
    """Define an auto-converting field holding a map of values"""
    subconverter = auto_converter(typ)
    keyconverter = auto_converter(key)
    converter = attrs.Converter(
        lambda d, self: {
            keyconverter(k, self): subconverter(v, self)
            for k, v in d.items()
        },
        takes_self=True,
    )
    return attrs.field(converter=converter, **kwargs)

##############################################################################
#
# iPXE algorithm mappings
#

class Algorithm(Enum):
    """An iPXE algorithm"""

    @property
    def basename(self):
        """Base algorithm name"""
        return self.name.lower()

    @property
    def symbol(self):
        """Algorithm symbol"""
        return "%s_algorithm" % self.basename

    @property
    def label(self):
        """Label for use in comments"""
        return self.value

class CipherAlgorithm(Algorithm):
    """An iPXE cipher algorithm"""
    AES_GCM = "AES-GCM"

class DigestAlgorithm(Algorithm):
    """An iPXE digest algorithm"""
    SHA1 = "SHA-1"
    SHA224 = "SHA-224"
    SHA256 = "SHA-256"
    SHA384 = "SHA-384"
    SHA512 = "SHA-512"
    SHA512_224 = "SHA-512/224"
    SHA512_256 = "SHA-512/256"

class HmacDigestAlgorithm(Algorithm):
    """An iPXE digest algorithm used with HMAC"""
    SHA1 = "HMACSHA1"
    SHA224 = "HMACSHA224"
    SHA256 = "HMACSHA256"
    SHA384 = "HMACSHA384"
    SHA512 = "HMACSHA512"
    SHA512_224 = "HMACSHA512/224"
    SHA512_256 = "HMACSHA512/256"

    @property
    def label(self):
        """Label for use in comments"""
        return "HMAC-%s" % self.name.replace("_", "/")

class HkdfDigestAlgorithm(Algorithm):
    """An iPXE digest algorithm used with HKDF"""
    SHA1 = "HKDF-SHA-1"
    SHA256 = "HKDF-SHA-256"
    SHA384 = "HKDF-SHA-384"
    SHA512 = "HKDF-SHA-512"

    @property
    def label(self):
        """Label for use in comments"""
        return "HKDF-%s" % self.name

class DiffieHellmanAlgorithm(Algorithm):
    """A Diffie-Hellman key exchange algorithm family"""
    ECDH = "ECDH"
    XDH = "XDH"

class ExchangeAlgorithm(Algorithm):
    """An iPXE key exchange algorithm"""
    P256 = "secp256r1"
    P384 = "secp384r1"
    X25519 = "curve25519"

    @property
    def label(self):
        """Label for use in comments"""
        return self.name

class EncryptionAlgorithm(Algorithm):
    """An iPXE public-key encryption algorithm"""
    RSA = "RSAES-PKCS1-v1_5"

class SignatureAlgorithm(Algorithm):
    """An iPXE public-key signature algorithm"""
    ECDSA = "ECDSA"
    RSA = "RSASSA-PKCS1-v1_5"
    RSA_PSS = "RSASSA-PSS"

##############################################################################
#
# Common types
#

class HexBytes(bytes):
    """A hex-encoded byte string"""

    def __new__(cls, value=b''):
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

class TestFlag(Enum):
    """A test flag"""
    ADDITION_CHAIN = "AdditionChain"
    ARITHMETIC_ERROR = "ArithmeticError"
    BER_ENCODED_PADDING = "BerEncodedPadding"
    BER_ENCODED_SIGNATURE = "BerEncodedSignature"
    COMPRESSED_POINT = "CompressedPoint"
    COMPRESSED_PUBLIC = "CompressedPublic"
    COUNTER_WRAP = "CounterWrap"
    CVS_2017_8932 = "CVE-2017-8932"
    CVE_2020_14967 = "CVE-2020-14967"
    CVE_2021_3580 = "CVE-2021-3580"
    EDGE_CASE_DOUBLING = "EdgeCaseDoubling"
    EDGE_CASE_EPHEMERAL_KEY = "EdgeCaseEphemeralKey"
    EDGE_CASE_MULTIPLICATION = "EdgeCaseMultiplication"
    EDGE_CASE_PRIVATE_KEY = "EdgeCasePrivateKey"
    EDGE_CASE_PUBLIC_KEY = "EdgeCasePublicKey"
    EDGE_CASE_SHAMIR_MULTIPLICATION = "EdgeCaseShamirMultiplication"
    EDGE_CASE_SHARED = "EdgeCaseShared"
    EDGE_CASE_SHARED_SECRET = "EdgeCaseSharedSecret"
    EDGE_CASE_SIGNAGTURE = "EdgeCaseSignature"
    EMPTY_SALT = "EmptySalt"
    INTEGER_OVERFLOW = "IntegerOverflow"
    INVALID_ASN_IN_PADDING = "InvalidAsnInPadding"
    INVALID_CIPHERTEXT_FORMAT = "InvalidCiphertextFormat"
    INVALID_COMPRESSED_PUBLIC = "InvalidCompressedPublic"
    INVALID_CURVE_ATTACK = "InvalidCurveAttack"
    INVALID_ENCODING = "InvalidEncoding"
    INVALID_PADDING = "InvalidPadding"
    INVALID_PKCS1_PADDING = "InvalidPkcs1Padding"
    INVALID_SIGNATURE = "InvalidSignature"
    INVALID_TYPES_IN_SIGNATURE = "InvalidTypesInSignature"
    KTV = "Ktv"
    LONG_IV = "LongIv"
    LOW_ORDER_PUBLIC = "LowOrderPublic"
    MAXIMAL_OUTPUT_SIZE = "MaximalOutputSize"
    MISSING_NULL = "MissingNull"
    MISSING_ZERO = "MissingZero"
    MODIFIED_INTEGER = "ModifiedInteger"
    MODIFIED_PADDING = "ModifiedPadding"
    MODIFIED_TAG = "ModifiedTag"
    MODIFIED_SIGNATURE = "ModifiedSignature"
    MODULAR_INVERSE = "ModularInverse"
    NO_HASH = "NoHash"
    NON_CANONICAL_PUBLIC = "NonCanonicalPublic"
    NORMAL = "Normal"
    OUTPUT_COLLISION = "OutputCollision"
    POINT_DUPLICATION = "PointDuplication"
    PSEUDORANDOM = "Pseudorandom"
    RANGE_CHECK = "RangeCheck"
    SHORT_PADDING = "ShortPadding"
    SIGNATURE_MALLEABILITY = "SignatureMalleability"
    SIZE_TOO_LARGE = "SizeTooLarge"
    SMALL_IV = "SmallIv"
    SMALL_MODULUS = "SmallModulus"
    SMALL_PUBLIC_KEY = "SmallPublicKey"
    SMALL_RAND_S = "SmallRandS"
    SMALL_SIGNATURE = "SmallSignature"
    SPECIAL_CASE_HASH = "SpecialCaseHash"
    SPECIAL_CASE_PADDING = "SpecialCasePadding"
    SPECIAL_CASE = "SpecialCase"
    SPECIAL_PUBLIC_KEY = "SpecialPublicKey"
    SSLV23_PADDING = "Sslv23Padding"
    TRUNCATED_HMAC = "TruncatedHmac"
    TWIST = "Twist"
    UNTRUNCATED_HASH = "Untruncatedhash"
    VALID_SIGNATURE = "ValidSignature"
    WEAK_HASH = "WeakHash"
    WRONG_CURVE = "WrongCurve"
    WRONG_HASH = "WrongHash"
    WRONG_PRIMITIVE = "WrongPrimitive"
    ZERO_LENGTH_IV = "ZeroLengthIv"
    ZERO_SHARED_SECRET = "ZeroSharedSecret"

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
class TestNamedObject:
    """A test named object"""

    @property
    @abstractmethod
    def test_file(self):
        """Containing test file"""

    @property
    def stable_props(self):
        """Get properties for constructing a stable ID"""
        props = {
            field.name.encode(): str(getattr(self, field.name)).encode()
            for field in attrs.fields(type(self))
            if field.metadata.get("stable")
        }
        return props

    @property
    def stable_id(self):
        """Calculate a stable test ID

        The tcId provides a stable ordering but not a stable
        numbering.  To avoid unnecessary churn when tests are
        renumbered upstream, we construct a stable identifier built
        from the relevant parameters of the test itself.
        """
        props = self.stable_props
        digest = hashlib.sha256()
        digest.update(b":".join(itertools.chain(
            (
                b"algorithm:%s" % algorithm.basename.encode()
                for algorithm in self.test_file.algorithms
            ),
            (
                b"%s:%d:%s" % (name, len(value), value)
                for name, value in sorted(props.items())
            ),
        )))
        return digest.hexdigest()[:8]

    @property
    def test_name(self):
        """Test case name"""
        return "wycheproof_%s_%s" % (self.test_file.basename, self.stable_id)

@attrclass
class TestCase(TestNamedObject):
    """A test case"""
    _parent = scalar_field(weakref.proxy, alias="_parent")
    comment = scalar_field(str)
    flags = set_field(TestFlag)
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

    def definition(self):
        """Generate source code for test definition

        We include the tcId within the source code comment to enable
        easy traceability of any failures back to the upstream test
        case.
        """
        skip = self.skip
        code = (
            "/* %s test case %d" % (self.test_file.label, self.tcId) +
            (" (skipped: %s)" % skip if skip else "") +
            " */\n"
        )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        return ""

@attrclass
class TestGroup(TestNamedObject):
    """A test group"""
    _parent = scalar_field(weakref.proxy, alias="_parent")
    source = scalar_field(TestSource)
    tests = list_field(TestCase)
    type = scalar_field(str)

    @property
    def test_file(self):
        """Containing test file"""
        return self._parent

    def definition(self):
        """Generate source code for test group definition"""
        code = "\n".join(x.definition() for x in self.tests)
        return code

@attrclass
class TestFile:
    """A test file"""
    SCHEMA: ClassVar = None
    filename = scalar_field(str)
    algorithm = scalar_field(Algorithm)
    header = list_field(str, factory=list)
    notes = map_field(TestNote, key=TestFlag, factory=dict)
    numberOfTests = scalar_field(int)
    schema = scalar_field(str)
    testGroups = list_field(TestGroup)

    @schema.validator
    def validate_schema(self, attr, value):
        """Validate schema"""
        if value != self.SCHEMA:
            raise ValueError(
                "%s: found schema %s (expected %s)" %
                (self.filename, value, self.SCHEMA)
            )

    @numberOfTests.validator
    def validate_number_of_tests(self, attr, value):
        """Validate number of tests"""
        if value != len(self.tests):
            raise ValueError(
                "%s: found %d tests (expected %d)" %
                (self.filename, len(self.tests), value)
            )

    @property
    def algorithms(self):
        """All algorithms"""
        return [self.algorithm]

    @property
    def basename(self):
        """Base name for test cases"""
        return "_".join(x.basename for x in self.algorithms)

    @property
    def label(self):
        """Label for use in comments"""
        return " / ".join(x.label for x in self.algorithms)

    @property
    def tests(self):
        """All test cases"""
        return [test for group in self.testGroups for test in group.tests]

    def definition(self):
        """Generate source code for test file definition"""
        code = "\n".join(x.definition() for x in self.testGroups)
        return code

    def source(self):
        """Generate source code"""
        generator = Path(__file__).name
        testname = "wycheproof_%s" % self.basename
        execname = "%s_exec" % testname
        label = self.label
        definitions = self.definition()
        invocations = "".join(x.invocation() for x in self.tests if not x.skip)
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
            static void {execname} ( void ) {{

            \t/* Perform tests in tcId order */
            """) +
            invocations +
            "}\n" +
            textwrap.dedent(f"""
            /** Wycheproof {label} self-tests */
            struct self_test {testname} __self_test = {{
            \t.name = "{testname}",
            \t.exec = {execname},
            }};
            REQUIRING_SYMBOL ( {testname} );
            """)
        )
        return code

    @classmethod
    def load(cls, fh, **kwargs):
        """Load from JSON input file handle"""
        data = json.load(fh)
        return cls(**data, **kwargs)

    @classmethod
    def read(cls, filename, **kwargs):
        """Read from JSON input file"""
        with open(filename, "rt") as fh:
            return cls.load(fh, filename=filename, **kwargs)

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
        self.validate_padded(attr, value, self.test_file.privsize)

    @public.validator
    def validate_public(self, attr, value):
        """Validate public key size"""
        self.validate_fixed(attr, value, self.test_file.pubsize)

    @shared.validator
    def validate_shared(self, attr, value):
        """Validate shared key size"""
        if not self.failure:
            self.validate_fixed(attr, value, self.test_file.sharedsize)

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        if not self.skip:
            algorithm = self.test_group.curve.symbol
            privsize = self.test_file.privsize
            pubsize = self.test_file.pubsize
            sharedsize = self.test_file.sharedsize
            code += (
                "EXCHANGE_TEST ( %s, &%s,\n" % (self.test_name, algorithm) +
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
    curve = scalar_field(ExchangeAlgorithm)
    tests = list_field(ExchangeTestCase)

@attrclass
class ExchangeTestFile(TestFile):
    """A key exchange test file"""
    privsize = scalar_field(int)
    pubsize = scalar_field(int)
    sharedsize = scalar_field(int)
    algorithm = scalar_field(DiffieHellmanAlgorithm)
    testGroups = list_field(ExchangeTestGroup)

    @property
    def algorithms(self):
        """All algorithms"""
        return sorted({x.curve for x in self.testGroups})

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
    encoding = scalar_field(str)
    tests = list_field(NistExchangeTestCase)

@attrclass
class NistExchangeTestFile(ExchangeTestFile):
    """A NIST elliptic curve key exchange test file"""
    SCHEMA: ClassVar = "ecdh_ecpoint_test_schema_v1.json"
    testGroups = list_field(NistExchangeTestGroup)

##############################################################################
#
# XDH key exchange tests
#

@attrclass
class XdhTestCase(ExchangeTestCase):
    """An XDH key exchange test case"""

    @property
    def failure(self):
        """Check if test case is expected to fail"""
        return TestFlag.ZERO_SHARED_SECRET in self.flags

@attrclass
class XdhTestGroup(ExchangeTestGroup):
    """An XDH key exchange test group"""
    tests = list_field(XdhTestCase)

@attrclass
class XdhTestFile(ExchangeTestFile):
    """An XDH key exchange test file"""
    SCHEMA: ClassVar = "xdh_comp_schema_v1.json"
    testGroups = list_field(XdhTestGroup)

##############################################################################
#
# HMAC tests
#

@attrclass
class HmacTestCase(TestCase):
    """An HMAC test case"""
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
        if TestFlag.MODIFIED_TAG in self.flags:
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
            algorithm = self.test_file.algorithm.symbol
            code += (
                "HMAC_TEST ( %s, &%s,\n" % (self.test_name, algorithm) +
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
    algorithm = scalar_field(HmacDigestAlgorithm)
    testGroups = list_field(HmacTestGroup)

    @property
    def basename(self):
        """Base name for test cases"""
        return "hmac_%s" % super().basename

##############################################################################
#
# HKDF tests
#

@attrclass
class HkdfTestCase(TestCase):
    """An HKDF test case"""
    ikm = scalar_field(HexBytes, metadata={"stable": True})
    salt = scalar_field(HexBytes, metadata={"stable": True})
    info = scalar_field(HexBytes, metadata={"stable": True})
    size = scalar_field(int, metadata={"stable": True})
    okm = scalar_field(HexBytes)

    @ikm.validator
    def validate_ikm(self, attr, value):
        """Validate key size"""
        self.validate_fixed(attr, value, (self.test_group.keySize // 8))

    @property
    def skip(self):
        """Reason for skipping test (if any)"""
        if TestFlag.SIZE_TOO_LARGE in self.flags:
            # Our HKDF abstraction does not perform runtime checks for
            # the output key material size
            return "size too large"

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        if not self.skip:
            algorithm = self.test_file.algorithm.symbol
            salted = (len(self.salt) > 0)
            code += (
                "HKDF_TEST ( %s, &%s, %d,\n" % (
                    self.test_name, algorithm, salted
                ) +
                self.ikm.source("\tIKM") + ",\n" +
                self.salt.source("\tSALT") + ",\n" +
                self.info.source("\tINFO") + ",\n" +
                "\tPRK_UNSPECIFIED,\n" +
                self.okm.source("\tOKM") + " );\n"
            )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        code += "\thkdf_ok ( &%s );\n" % self.test_name
        return code

@attrclass
class HkdfTestGroup(TestGroup):
    """An HKDF test group"""
    keySize = scalar_field(int)
    tests = list_field(HkdfTestCase)

@attrclass
class HkdfTestFile(TestFile):
    """An HKDF test file"""
    SCHEMA: ClassVar = "hkdf_test_schema_v1.json"
    algorithm = scalar_field(HkdfDigestAlgorithm)
    testGroups = list_field(HkdfTestGroup)

    @property
    def basename(self):
        """Base name for test cases"""
        return "hkdf_%s" % super().basename

##############################################################################
#
# AEAD cipher tests
#

@attrclass
class AeadCipherTestCase(TestCase):
    """An AEAD cipher test case"""
    key = scalar_field(HexBytes, metadata={"stable": True})
    iv = scalar_field(HexBytes, metadata={"stable": True})
    aad = scalar_field(HexBytes, metadata={"stable": True})
    msg = scalar_field(HexBytes, metadata={"stable": True})
    ct = scalar_field(HexBytes)
    tag = scalar_field(HexBytes)

    @key.validator
    def validate_key(self, attr, value):
        """Validate key size"""
        self.validate_fixed(attr, value, (self.test_group.keySize // 8))

    @iv.validator
    def validate_iv(self, attr, value):
        """Validate IV size"""
        self.validate_fixed(attr, value, (self.test_group.ivSize // 8))

    @tag.validator
    def validate_tag(self, attr, value):
        """Validate tag size"""
        self.validate_fixed(attr, value, (self.test_group.tagSize // 8))

    @property
    def skip(self):
        """Reason for skipping test (if any)"""
        if TestFlag.MODIFIED_TAG in self.flags:
            # Our cipher abstraction covers only generating the tag,
            # not comparing the tag to check for a match
            return "modified tag"

    @property
    def key_failure(self):
        """Check if test case is expected to fail due to invalid key"""
        return False

    @property
    def iv_failure(self):
        """Check if test case is expected to fail due to invalid IV"""
        return False

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        if not self.skip:
            algorithm = self.test_file.algorithm.symbol
            code += (
                "CIPHER_TEST ( %s, &%s,\n" % (self.test_name, algorithm) +
                self.key.source("\tKEY") + ",\n" +
                self.iv.source("\tIV") + ",\n" +
                self.aad.source("\tADDITIONAL") + ",\n" +
                self.msg.source("\tPLAINTEXT") + ",\n" +
                self.ct.source("\tCIPHERTEXT") + ",\n" +
                self.tag.source("\tAUTH") + " );\n"
            )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        if self.key_failure:
            code += "\tcipher_key_fail_ok ( &%s );\n" % self.test_name
        elif self.iv_failure:
            code += "\tcipher_iv_fail_ok ( &%s );\n" % self.test_name
        elif self.failure:
            raise ValueError("%d: unknown cipher failure reason" % self.tcId)
        else:
            code += "\tcipher_ok ( &%s );\n" % self.test_name
        return code

@attrclass
class AeadCipherTestGroup(TestGroup):
    """An AEAD cipher test group"""
    ivSize = scalar_field(int)
    keySize = scalar_field(int)
    tagSize = scalar_field(int)
    tests = list_field(AeadCipherTestCase)

@attrclass
class AeadCipherTestFile(TestFile):
    """An AEAD cipher test file"""
    SCHEMA: ClassVar = "aead_test_schema_v1.json"
    algorithm = scalar_field(CipherAlgorithm)
    testGroups = list_field(AeadCipherTestGroup)

##############################################################################
#
# GCM cipher tests
#

@attrclass
class GcmCipherTestCase(AeadCipherTestCase):
    """A GCM cipher test case"""

    @property
    def iv_failure(self):
        """Check if test case is expected to fail due to invalid IV"""
        return TestFlag.ZERO_LENGTH_IV in self.flags

@attrclass
class GcmCipherTestGroup(AeadCipherTestGroup):
    """A GCM cipher test group"""
    tests = list_field(GcmCipherTestCase)

@attrclass
class GcmCipherTestFile(AeadCipherTestFile):
    """A GCM cipher test file"""
    testGroups = list_field(GcmCipherTestGroup)

##############################################################################
#
# RSA tests
#

@attrclass
class RsaTestGroup(TestGroup):
    """An RSA test group"""
    keySize = scalar_field(int)

    @property
    def private_key(self):
        """Private key"""
        return None

    @property
    def public_key(self):
        """Public key"""
        return None

    def definition(self):
        """Generate source code for test group definition"""
        algorithm = self.test_file.algorithm.symbol
        private_key = self.private_key or HexBytes()
        public_key = self.public_key or HexBytes()
        code = (
            "/* Key pair for following tests */\n" +
            "PUBKEY_TEST ( %s, &%s,\n" % (self.test_name, algorithm) +
            private_key.source("\tPRIVATE") + ",\n" +
            public_key.source("\tPUBLIC") + " );\n" +
            "\n" +
            super().definition()
        )
        return code

@attrclass
class RsaTestFile(TestFile):
    """An RSA test file"""
    testGroups = list_field(RsaTestGroup)

    @property
    def keysizes(self):
        """All key sizes"""
        return sorted({x.keySize for x in self.testGroups})

    @property
    def basename(self):
        """Base name for test cases"""
        return "_".join("%d" % x for x in self.keysizes)

    @property
    def label(self):
        """Label for use in comments"""
        return "%s-bit" % "/".join("%d" % x for x in self.keysizes)

##############################################################################
#
# RSA decryption tests
#

@attrclass
class RsaDecryptTestCase(TestCase):
    """An RSA decryption test case"""
    msg = scalar_field(HexBytes, metadata={"stable": True})
    ct = scalar_field(HexBytes, metadata={"stable": True})

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        code += (
            "PUBKEY_ENCRYPTION_TEST ( %s,\n" % self.test_name +
            "\t&%s, RANDOM(),\n" % self.test_group.test_name +
            self.msg.source("\tPLAINTEXT") + ",\n" +
            self.ct.source("\tCIPHERTEXT") + " );\n"
        )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        if self.failure:
            code += "\tpubkey_decrypt_fail_ok ( &%s );\n" % self.test_name
        else:
            code += "\tpubkey_decrypt_ok ( &%s );\n" % self.test_name
        return code

@attrclass
class RsaDecryptTestGroup(RsaTestGroup):
    """An RSA decryption test group"""
    privateKey = map_field(str) # ignored
    privateKeyPkcs8 = scalar_field(HexBytes, metadata={"stable": True})
    privateKeyPem = scalar_field(str) # ignored
    privateKeyJwk = map_field(str) # ignored
    tests = list_field(RsaDecryptTestCase)

    @property
    def private_key(self):
        """Private key"""
        return self.privateKeyPkcs8

@attrclass
class RsaDecryptTestFile(RsaTestFile):
    """An RSA decryption test file"""
    algorithm = scalar_field(EncryptionAlgorithm)
    testGroups = list_field(RsaDecryptTestGroup)

    @property
    def basename(self):
        """Base name for test cases"""
        return "%s_decrypt" % super().basename

@attrclass
class RsaPkcs1DecryptTestFile(RsaDecryptTestFile):
    """An RSA PKCS#1 decryption test file"""
    SCHEMA: ClassVar = "rsaes_pkcs1_decrypt_schema_v1.json"

    @property
    def basename(self):
        """Base name for test cases"""
        return "rsa_pkcs1_%s" % super().basename

    @property
    def label(self):
        """Label for use in comments"""
        return "RSA-PKCS#1 (%s)" % super().label

##############################################################################
#
# RSA signing tests
#

@attrclass
class RsaSignTestCase(TestCase):
    """An RSA signing test case"""
    msg = scalar_field(HexBytes, metadata={"stable": True})
    sig = scalar_field(HexBytes, metadata={"stable": True})

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        digest = self.test_group.sha.symbol
        code += (
            "PUBKEY_SIGNATURE_TEST ( %s,\n" % self.test_name +
            "\t&%s, RANDOM(),\n" % self.test_group.test_name +
            self.msg.source("\tPLAINTEXT") + ",\n" +
            "\t&%s,\n" % digest +
            self.sig.source("\tSIGNATURE") + " );\n"
        )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        code += "\tpubkey_sign_verify_ok ( &%s );\n" % self.test_name
        return code

@attrclass
class RsaSignTestGroup(RsaTestGroup):
    """An RSA signing test group"""
    privateKey = map_field(str) # ignored
    keyAsn = scalar_field(str) # ignored
    keyDer = scalar_field(HexBytes, metadata={"stable": True})
    keyJwk = map_field(str, factory=dict) # ignored
    keyPem = scalar_field(str) # ignored
    privateKeyJwk = map_field(str, factory=dict) # ignored
    privateKeyPem = scalar_field(str) # ignored
    privateKeyPkcs8 = scalar_field(HexBytes, metadata={"stable": True})
    sha = scalar_field(DigestAlgorithm)
    tests = list_field(RsaSignTestCase)

    @property
    def private_key(self):
        """Private key"""
        return self.privateKeyPkcs8

    @property
    def public_key(self):
        """Public key"""
        return self.keyDer

@attrclass
class RsaSignTestFile(RsaTestFile):
    """An RSA signing test file"""
    algorithm = scalar_field(SignatureAlgorithm)
    testGroups = list_field(RsaSignTestGroup)

    @property
    def basename(self):
        """Base name for test cases"""
        return "%s_sign" % super().basename

@attrclass
class RsaPkcs1SignTestFile(RsaSignTestFile):
    """An RSA PKCS#1 signing test file"""
    SCHEMA: ClassVar = "rsassa_pkcs1_generate_schema_v1.json"

    @property
    def basename(self):
        """Base name for test cases"""
        return "rsa_pkcs1_%s" % super().basename

    @property
    def label(self):
        """Label for use in comments"""
        return "RSA-PKCS#1 signing (%s)" % super().label

##############################################################################
#
# RSA verification tests
#

@attrclass
class RsaVerifyTestCase(TestCase):
    """An RSA verification test case"""
    msg = scalar_field(HexBytes, metadata={"stable": True})
    sig = scalar_field(HexBytes, metadata={"stable": True})

    @property
    def failure(self):
        """Check if test case is expected to fail"""
        failure = super().failure or TestFlag.MISSING_NULL in self.flags
        return failure

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        digest = self.test_group.sha.symbol
        code += (
            "PUBKEY_SIGNATURE_TEST ( %s,\n" % self.test_name +
            "\t&%s, RANDOM(),\n" % self.test_group.test_name +
            self.msg.source("\tPLAINTEXT") + ",\n" +
            "\t&%s,\n" % digest +
            self.sig.source("\tSIGNATURE") + " );\n"
        )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        if self.failure:
            code += "\tpubkey_verify_fail_ok ( &%s );\n" % self.test_name
        else:
            code += "\tpubkey_verify_ok ( &%s );\n" % self.test_name
        return code

@attrclass
class RsaVerifyTestGroup(RsaTestGroup):
    """An RSA verification test group"""
    publicKey = map_field(str) # ignored
    publicKeyAsn = scalar_field(str) # ignored
    publicKeyDer = scalar_field(HexBytes, metadata={"stable": True})
    publicKeyPem = scalar_field(str) # ignored
    publicKeyJwk = map_field(str, factory=dict) # ignored
    keyDer = scalar_field(HexBytes, default=None)
    keyJwk = map_field(str, factory=dict) # ignored
    sha = scalar_field(DigestAlgorithm)
    tests = list_field(RsaVerifyTestCase)

    @property
    def public_key(self):
        """Public key"""
        return self.publicKeyDer

@attrclass
class RsaVerifyTestFile(RsaTestFile):
    """An RSA verification test file"""
    algorithm = scalar_field(SignatureAlgorithm)
    testGroups = list_field(RsaVerifyTestGroup)

    @property
    def digests(self):
        """All digest algorithms"""
        return sorted({x.sha for x in self.testGroups})

    @property
    def basename(self):
        """Base name for test cases"""
        digests = "_".join(x.basename for x in self.digests)
        return "%s_%s_verify" % (super().basename, digests)

@attrclass
class RsaPkcs1VerifyTestFile(RsaVerifyTestFile):
    """An RSA PKCS#1 verification test file"""
    SCHEMA: ClassVar = "rsassa_pkcs1_verify_schema_v1.json"

    @property
    def basename(self):
        """Base name for test cases"""
        return "rsa_pkcs1_%s" % super().basename

    @property
    def label(self):
        """Label for use in comments"""
        digests = " / ".join(x.label for x in self.digests)
        return "RSA-PKCS#1 %s verification (%s)" % (digests, super().label)

    def source(self):
        """Generate source code"""
        code = super().source()
        code += "".join(
            "REQUIRE_OBJECT ( rsa_%s );\n" % x.basename for x in self.digests
        )
        return code

@attrclass
class RsaPssVerifyTestGroup(RsaVerifyTestGroup):
    """An RSA-PSS verification test group"""
    mgf = scalar_field(str)
    mgfSha = scalar_field(DigestAlgorithm)
    sLen = scalar_field(int)

@attrclass
class RsaPssVerifyTestFile(RsaVerifyTestFile):
    """An RSA-PSS verification test file"""
    SCHEMA: ClassVar = "rsassa_pss_verify_schema_v1.json"
    testGroups = list_field(RsaPssVerifyTestGroup)

    @property
    def basename(self):
        """Base name for test cases"""
        digests = "_".join(x.basename for x in self.digests)
        return "rsa_pss_%s" % super().basename

    @property
    def label(self):
        """Label for use in comments"""
        digests = " / ".join(x.label for x in self.digests)
        return "RSA-PSS %s verification (%s)" % (digests, super().label)

##############################################################################
#
# ECDSA verification tests
#

@attrclass
class EcdsaTestKey:
    """An ECDSA public key"""
    type = scalar_field(str)
    curve = scalar_field(ExchangeAlgorithm)
    keySize = scalar_field(int)
    uncompressed = scalar_field(HexBytes)
    wx = scalar_field(HexBytes)
    wy = scalar_field(HexBytes)

@attrclass
class EcdsaTestCase(TestCase):
    """An ECDSA test case"""
    msg = scalar_field(HexBytes, metadata={"stable": True})
    sig = scalar_field(HexBytes, metadata={"stable": True})

    @property
    def stable_props(self):
        """Get properties for constructing a stable ID"""
        props = super().stable_props
        props.update(self._parent.stable_props)
        return props

    def definition(self):
        """Generate source code for test definition"""
        code = super().definition()
        digest = self.test_group.sha.symbol
        code += (
            "PUBKEY_SIGNATURE_TEST ( %s,\n" % self.test_name +
            "\t&%s, RANDOM(),\n" % self.test_group.test_name +
            self.msg.source("\tPLAINTEXT") + ",\n" +
            "\t&%s,\n" % digest +
            self.sig.source("\tSIGNATURE") + " );\n"
        )
        return code

    def invocation(self):
        """Generate source code for test invocation"""
        code = super().invocation()
        if self.failure:
            code += "\tpubkey_verify_fail_ok ( &%s );\n" % self.test_name
        else:
            code += "\tpubkey_verify_ok ( &%s );\n" % self.test_name
        return code

@attrclass
class EcdsaTestGroup(TestGroup):
    """An ECDSA test group"""
    publicKey = scalar_field(EcdsaTestKey)
    publicKeyDer = scalar_field(HexBytes, metadata={"stable": True})
    publicKeyPem = scalar_field(str) # ignored
    sha = scalar_field(DigestAlgorithm)
    tests = list_field(EcdsaTestCase)

@attrclass
class EcdsaTestFile(TestFile):
    """An ECDSA test file"""
    SCHEMA: ClassVar = "ecdsa_verify_schema_v1.json"
    algorithm = scalar_field(SignatureAlgorithm)
    testGroups = list_field(EcdsaTestGroup)

    @property
    def curves(self):
        """All curves"""
        return sorted({x.publicKey.curve for x in self.testGroups})

    @property
    def digests(self):
        """All digest algorithms"""
        return sorted({x.sha for x in self.testGroups})

    @property
    def basename(self):
        """Base name for test cases"""
        curves = "_".join(x.basename for x in self.curves)
        digests = "_".join(x.basename for x in self.digests)
        return "%s_%s_verify" % (curves, digests)

    @property
    def label(self):
        """Label for use in comments"""
        curves = " / ".join(x.label for x in self.curves)
        digests = " / ".join(x.label for x in self.digests)
        return "ECDSA %s %s verification" % (curves, digests)

    def definition(self):
        """Generate source code for test file definition"""
        algorithm = self.algorithm.symbol
        keys = sorted({
            (x.test_name, x.publicKeyDer) for x in self.testGroups
        })
        code = (
            "\n".join(
                "/* Key pairs used by at least one following test */\n" +
                "PUBKEY_TEST ( %s, &%s,\n" % (name, algorithm) +
                "\tPRIVATE(),\n" +
                key.source("\tPUBLIC") + " );\n"
                for name, key in keys
            ) + "\n" +
            super().definition()
        )
        return code

    def source(self):
        """Generate source code"""
        code = super().source()
        code += "".join(
            "REQUIRE_OBJECT ( %s );\n" % x.basename
            for x in self.curves
        )
        return code

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
    tests = [
        EcdsaTestFile.read(srcdir / "ecdsa_secp256r1_sha256_test.json"),
        EcdsaTestFile.read(srcdir / "ecdsa_secp256r1_sha512_test.json"),
        EcdsaTestFile.read(srcdir / "ecdsa_secp384r1_sha256_test.json"),
        EcdsaTestFile.read(srcdir / "ecdsa_secp384r1_sha384_test.json"),
        EcdsaTestFile.read(srcdir / "ecdsa_secp384r1_sha512_test.json"),
        GcmCipherTestFile.read(srcdir / "aes_gcm_test.json"),
        HkdfTestFile.read(srcdir / "hkdf_sha1_test.json"),
        HkdfTestFile.read(srcdir / "hkdf_sha256_test.json"),
        HkdfTestFile.read(srcdir / "hkdf_sha384_test.json"),
        HkdfTestFile.read(srcdir / "hkdf_sha512_test.json"),
        HmacTestFile.read(srcdir / "hmac_sha1_test.json"),
        HmacTestFile.read(srcdir / "hmac_sha224_test.json"),
        HmacTestFile.read(srcdir / "hmac_sha256_test.json"),
        HmacTestFile.read(srcdir / "hmac_sha384_test.json"),
        HmacTestFile.read(srcdir / "hmac_sha512_test.json"),
        HmacTestFile.read(srcdir / "hmac_sha512_224_test.json"),
        HmacTestFile.read(srcdir / "hmac_sha512_256_test.json"),
        NistExchangeTestFile.read(
            srcdir / "ecdh_secp256r1_ecpoint_test.json",
            privsize=32, pubsize=65, sharedsize=32,
        ),
        NistExchangeTestFile.read(
            srcdir / "ecdh_secp384r1_ecpoint_test.json",
            privsize=48, pubsize=97, sharedsize=48,
        ),
        RsaPkcs1DecryptTestFile.read(srcdir / "rsa_pkcs1_2048_test.json"),
        RsaPkcs1DecryptTestFile.read(srcdir / "rsa_pkcs1_3072_test.json"),
        RsaPkcs1DecryptTestFile.read(srcdir / "rsa_pkcs1_4096_test.json"),
        RsaPkcs1SignTestFile.read(srcdir / "rsa_pkcs1_1024_sig_gen_test.json"),
        RsaPkcs1SignTestFile.read(srcdir / "rsa_pkcs1_1536_sig_gen_test.json"),
        RsaPkcs1SignTestFile.read(srcdir / "rsa_pkcs1_2048_sig_gen_test.json"),
        RsaPkcs1SignTestFile.read(srcdir / "rsa_pkcs1_3072_sig_gen_test.json"),
        RsaPkcs1SignTestFile.read(srcdir / "rsa_pkcs1_4096_sig_gen_test.json"),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_2048_sha224_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_2048_sha256_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_2048_sha384_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_2048_sha512_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_2048_sha512_224_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_2048_sha512_256_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_3072_sha256_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_3072_sha384_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_3072_sha512_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_3072_sha512_256_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_4096_sha256_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_4096_sha384_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_4096_sha512_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_4096_sha512_256_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_8192_sha256_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_8192_sha384_test.json"
        ),
        RsaPkcs1VerifyTestFile.read(
            srcdir / "rsa_signature_8192_sha512_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_2048_sha1_mgf1_20_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_2048_sha256_mgf1_32_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_2048_sha384_mgf1_48_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_2048_sha512_224_mgf1_28_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_2048_sha512_256_mgf1_32_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_3072_sha256_mgf1_32_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_4096_sha256_mgf1_32_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_4096_sha384_mgf1_48_test.json"
        ),
        RsaPssVerifyTestFile.read(
            srcdir / "rsa_pss_4096_sha512_mgf1_64_test.json"
        ),
        XdhTestFile.read(
            srcdir / "x25519_test.json",
            privsize=32, pubsize=32, sharedsize=32,
        ),
    ]

    # Write source code outputs
    for test in tests:
        test.write(dstdir / ("wycheproof_%s.c" % test.basename))

if __name__ == "__main__":
    main()
