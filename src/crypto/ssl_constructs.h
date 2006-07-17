// Note: This file still needs some work.
// Note: I had to redefine the enums to a set of const values,
//   so that the size of the variable would be correct.

// Typedefs
// (As defined by the SSL v3.0 RFC Draft)
// URL: http://wp.netscape.com/eng/ssl3/draft302.txt
typedef unsigned char uint8;
typedef uint8 uint16[2];
typedef uint8 uint24[3];
typedef uint8 uint32[4];
typedef uint8 uint64[8];

// Record layers
typedef struct _ProtocolVersion{
  uint8 major, minor;
} ProtocolVersion;

const ProtocolVersion version = { 3, 0 };

typedef uint8 ContentType;
const ContentType content_type_change_cipher_spec_type = 20;
const ContentType content_type_alert = 21;
const ContentType content_type_handshake = 22;
const ContentType content_type_application_data = 23;

typedef struct _SSLPlaintext{
  ContentType type;
  ProtocolVersion version;
  uint16 length; // can not exceed 2^14 bytes
  uint8 fragment[16384]; // 2^14 = 16,384 bytes
} SSLPlaintext;

typedef struct _SSLCompressed{
  ContentType type;
  ProtocolVersion version;
  uint16 length; // can not exceed 2^14 + 1024
  uint8 fragment[17408]; // SSLCompressed.length
} SSLCompressed;

typedef struct _SSLCiphertext{
  ContentType type;
  ProtocolVersion version;
  uint16 length;
  uint8 fragment; // so we have a pointer to the data, and don't have to do math
  // fragment; type GenericStreamCipher or GenericBlockCipher
} SSLCiphertext; // recast to get fragment

typedef struct _GenericStreamCipher{
  uint8 content[17408]; // SSLCompressed.length
  uint8 MAC[]; // CipherSpec.hash_size
} GenericStreamCipher;

typedef struct _SSLStreamCiphertext{
  ContentType type;
  ProtocolVersion version;
  uint16 length; // can not exceed 2^14 + 2048 = 18,456
  GenericStreamCipher fragment;
} SSLStreamCiphertext;

typedef struct _GenericBlockCipher{
  uint8 content[17408]; // SSLConpressed.length
  uint8 MAC[0]; // CipherSpec.hash_size
  // padding is used to bring the plaintext to
  // a multiple of the block cipher's block length.
  uint8 padding[0]; // GenericBlockCipher.padding_length
  uint8 padding_length;
} GenericBlockCipher;

typedef struct _SSLBlockCiphertext{
  ContentType type;
  ProtocolVersion version;
  uint16 length; // can not exceed 2^14 + 2048 = 18,456
  GenericBlockCipher fragment;
} SSLBlockCiphertext;

// Change cipher specs message
typedef struct _ChangeCipherSpec{
  enum { type_change_cipher_spec=1, type_size=255 } type;
} ChangeCipherSpec;

// Alert messages
typedef uint8 AlertLevel;
const AlertLevel alert_level_warning = 1;
const AlertLevel alert_level_fatal=2;

typedef uint8 AlertDescription;
const AlertDescription alert_description_close_notify = 0;
const AlertDescription alert_description_unexpected_message = 10;
const AlertDescription alert_description_bad_record_mac = 20;
const AlertDescription alert_description_decompression_failure = 30;
const AlertDescription alert_description_handshake_failure = 40;
const AlertDescription alert_description_no_certificate = 41;
const AlertDescription alert_description_bad_certificate = 42;
const AlertDescription alert_description_unsupported_certificate = 43;
const AlertDescription alert_description_certificate_revoked = 44;
const AlertDescription alert_description_certificate_expired = 45;
const AlertDescription alert_description_certificate_unknown = 46;
const AlertDescription alert_description_illegal_parameter = 47;

typedef struct _Alert{
  AlertLevel level;
  AlertDescription description;
} Alert;

// Handshake protocol
// What is the best way to have a generic pointer to the body struct??
typedef uint8 HandshakeType;
const HandshakeType handshake_type_hello_request = 0;
const HandshakeType handshake_type_client_hello = 1;
const HandshakeType handshake_type_server_hello = 2;
const HandshakeType handshake_type_certificate = 11;
const HandshakeType handshake_type_server_key_exchange = 12;
const HandshakeType handshake_type_certificate_request = 13;
const HandshakeType handshake_type_server_done = 14;
const HandshakeType handshake_type_certificate_verify = 15;
const HandshakeType handshake_type_client_key_exchange = 16;
const HandshakeType handshake_type_finished = 20;

typedef struct _Handshake{
  HandshakeType msg_type;
  uint24 length;
  // body; // one of HandshakeType structs
} Handshake; // generic Handshake, need to recast to get body

// Hello messages
typedef struct _HelloRequest{} HelloRequest;

typedef struct _HelloRequestHandshake{
  HandshakeType msg_type;
  uint24 length;
  HelloRequest body;
} HelloRequestHandshake;

typedef struct _Random{
  uint32 gmt_unix_time;
  uint8 random_bytes[28];
} Random;

//typedef uint8 SessionID[32]; // <0..32>
typedef uint8 SessionIDLength;
typedef uint8 SessionID;

typedef uint16 CipherSuiteLength;
typedef uint8 CipherSuite[2];

typedef uint8 CompressionMethodLength;
typedef uint8 CompressionMethod;
const CompressionMethod compression_method_null = 0;


typedef struct _ClientHello{
  ProtocolVersion client_version;
  Random random;
  SessionIDLength session_id_length;
  SessionID *session_id;
  SessionID *session_id_end;
  CipherSuiteLength *cipher_suites_length;
  CipherSuite *cipher_suites; // min size is one entry
  CipherSuite *cipher_suites_end;
  //CipherSuite cipher_suites[32768]; // <2..2^16-1> = 65,536 bytes and CipherSuite is 2 bytes
  CompressionMethodLength *compression_methods_length;
  CompressionMethod *compression_methods;
  CompressionMethod *compression_methods_end;
  //CompressionMethod *compression_methods; // min size is zero
  //CompressionMethod compression_methods[256]; // <0..2^8-1> = 256 bytes and CompressionMethod is 1 byte
} ClientHello;

typedef struct _ClientHelloHandshake{
  //HandshakeType msg_type;
  uint8 msg_type;
  uint24 length;
  ClientHello body;
} ClientHelloHandshake;

typedef struct _ServerHello{
  ProtocolVersion server_version;
  Random random;
  SessionID session_id;
  CipherSuite cipher_suite;
  CompressionMethod compression_method;
} ServerHello;

typedef struct _ServerHelloHandshake{
  HandshakeType msg_type;
  uint24 length;
  ServerHello body;
} ServerHelloHandshake;

// Server authentication and key exchange messages
typedef uint8 ASN1Cert[16777216]; // <1..2^24-1> = 16,777,216 bytes

typedef struct _Certificate{
  ASN1Cert certificate_list[1]; // <1..2^24-1> / ANS1Cert = 1
  // for some reason the size of certificate_list and ASN1Cert is the same, so only one certificate in the list
} Certificate;

typedef uint8 KeyExchangeAlgorithm;
const KeyExchangeAlgorithm key_exchange_algorithm_rsa = 0;
const KeyExchangeAlgorithm key_exchange_algorithm_diffie_hellman = 1;
const KeyExchangeAlgorithm key_exchange_algorithm_fortezza_kea = 2;

typedef struct _AnonSignature{
  struct {};
} AnonSignature;

typedef struct _RSASignature{
  uint8 md5_hash[16];
  uint8 sha_hash[20];
} RSASignature;

typedef struct _DSASignature{
  uint8 sha_hash[20];
} DSASignature;

// use union??,  make a mess to reference, but easy to make Signature type.
typedef union _Signature{ AnonSignature anon; RSASignature rsa; DSASignature dsa; } Signature;

typedef struct _ServerRSAParams{
  uint8 RSA_modulus[65536]; // <1..2^16-1> = 65,536
  uint8 RSA_exponent[65536]; // <1..2^16-1> = 65,536
} ServerRSAParams;

typedef struct _ServerDHParams{
  uint8 DH_p[65536]; // <1..2^16-1>
  uint8 DH_g[65536]; // <1..2^16-1>
  uint8 DH_Ys[65536]; // <1..2^16-1>
} ServerDHParams;

typedef struct _ServerDHKeyExchange{
  ServerDHParams params;
  Signature signed_params;
} ServerDHKeyExchange;

typedef struct _ServerRSAKeyExchange{
  ServerRSAParams params;
  Signature signed_params;
} ServerRSAKeyExchange;

typedef uint8 SignatureAlgorithm;
const SignatureAlgorithm signature_algorithm_anonymous = 0;
const SignatureAlgorithm signature_algorithm_rsa = 1;
const SignatureAlgorithm signature_algorithm_dsa = 2;

typedef uint8 CertificateType;
const CertificateType certificate_type_RSA_sign = 1;
const CertificateType certificate_type_DSS_sign = 2;
const CertificateType certificate_type_RSA_fixed_DH = 3;
const CertificateType certificate_type_DSS_fixed_DH = 4;
const CertificateType certificate_type_RSA_ephemeral_DH = 5;
const CertificateType certificate_type_DSS_ephemeral_DH = 6;
const CertificateType certificate_type_FORTEZZA_MISSI = 20;

typedef uint8 DistinguishedName[65536]; // <1..2^16-1> = 65,536

typedef struct _CertificateRequest{
  CertificateType certificate_types[256]; // <1..2^8-1>
  DistinguishedName certificate_authorities[1]; // <3...2^16-1> / DistinguishedName
  // this is another one that is odd with a list size of 1
} CertificateRequest;

typedef struct _ServerHelloDone{} ServerHelloDone;

// Client authentication and key exchange messages
typedef struct _PreMasterSecret{
  ProtocolVersion client_version;
  uint8 random[46];
} PreMasterSecret;

typedef struct _EncryptedPreMasterSecret{
  PreMasterSecret pre_master_secret;
} EncryptedPreMasterSecret;

typedef struct _RSAClientKeyExchange{
  EncryptedPreMasterSecret exchange_keys;
} RSAClientKeyExchange;

typedef uint8 PublicValueEncoding;
const PublicValueEncoding public_value_encoding_implicit = 0;
const PublicValueEncoding public_value_encoding_explicit = 1;

typedef struct _ClientDiffieHellmanPublic{
  // This is a select on PublicValueEncoding,  and I chose the larger size
  uint8 dh_public[65536]; // DH_Yc<1..2^16-1>, the dh public value
} ClientDiffieHellmanPublic;

typedef struct _DHClientKeyExhange{
  ClientDiffieHellmanPublic exchange_keys;
} DHClientKeyExchange;

typedef struct _CertificateVerify{
  Signature signature;
} CertificateVerify;

// Handshake finalization message
typedef struct _Finished{
  uint8 md5_hash[16];
  uint8 sha_hash[20];
} Finished;

// The CipherSuite
CipherSuite SSL_NULL_WITH_NULL_NULL                     = { 0x00, 0x13 };
CipherSuite SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA        = { 0x00, 0x0B };
CipherSuite SSL_DH_DSS_WITH_DES_CBC_SHA                 = { 0x00, 0x0C };
CipherSuite SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA       = { 0x00, 0x11 };
CipherSuite SSL_DH_anon_EXPORT_WITH_RC4_40_MD5          = { 0x00, 0x17 };
CipherSuite SSL_DH_anon_WITH_RC4_128_MD5                = { 0x00, 0x18 };

// The CipherSpec
typedef uint8 CipherType;
const CipherType cipher_type_stream = 0;
const CipherType cipher_type_block = 1;

typedef uint8 IsExportable;
const IsExportable is_exportable_true = 0;
const IsExportable is_exportable_false = 1;

typedef uint8 BulkCipherAlgorithm;
const BulkCipherAlgorithm bulk_cipher_algorithm_null = 0;
const BulkCipherAlgorithm bulk_cipher_algorithm_rc4 = 1;
const BulkCipherAlgorithm bulk_cipher_algorithm_rc2 = 2;
const BulkCipherAlgorithm bulk_cipher_algorithm_des = 3;
const BulkCipherAlgorithm bulk_cipher_algorithm_3des = 4;
const BulkCipherAlgorithm bulk_cipher_algorithm_des40 = 5;
const BulkCipherAlgorithm bulk_cipher_algorithm_fortezza = 6;

typedef uint8 MACAlgorithm;
const MACAlgorithm mac_algorithm_null = 0;
const MACAlgorithm mac_algorithm_md5 = 1;
const MACAlgorithm mac_algorithm_sha = 2;

typedef struct _CipherSpec{
  BulkCipherAlgorithm bulk_cipher_algorithm;
  MACAlgorithm mac_algorithm;
  CipherType cipher_type;
  IsExportable is_exportable;
  uint8 hash_size;
  uint8 key_material;
  uint8 IV_size;
} CipherSpec;


