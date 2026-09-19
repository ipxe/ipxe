#ifndef _IPXE_TLSFMT_H
#define _IPXE_TLSFMT_H

/** @file
 *
 * TLS data formats
 *
 * TLS uses an ad hoc mixture of fixed-length and variable-length
 * fields.  Variable-length fields are preceded by a length field that
 * may be one, two, or three bytes depending on the maximum length
 * defined by the structure.
 *
 * To avoid open-coding a very large number of bounds checks, we
 * define an abstraction for decomposing the component parts of a TLS
 * data structure into a sequence of field data pointers and lengths
 * (for variable-length fields), along with an efficient binary
 * encoding that can describe the mapping between the decomposition
 * and the raw data structure.
 *
 * A fixed-length field is described using a simple pointer to the
 * appropriate fixed-length data type.  A variable-length field is
 * described using a cursor structure that comprises a void pointer
 * followed by a length.  TLS extensions are always variable-length
 * and so are represented in the same way as variable-length fields.
 *
 * For example, the start of a ServerHello could be described using:
 *
 *     struct tls_server_hello {
 *         uint16_t *version;
 *         struct tls_random *random;
 *         struct tls_cursor session_id;
 *         uint16_t *suite;
 *         uint8_t *compression;
 *         struct {
 *             struct tls_cursor all;
 *             struct tls_cursor renegotiation;
 *             struct tls_cursor extended_master_secret;
 *         } ext;
 *     };
 *
 * Note that the fixed-length fields are all typed pointers, whereas
 * the variable-length session ID uses a TLS cursor (i.e. a void
 * pointer and a length).  Since pointer values and length values are
 * necessarily the same size, we can meaningfully treat this
 * descriptor structure as an array `ptrlen[]` of pointer/length
 * values.
 *
 * We create a binary encoding to allow us to define the mapping
 * between this descriptor structure and the raw TLS data structure
 * using a static byte array constructed at build time.  The same
 * binary encoding may be used both for parsing and for building a TLS
 * data structure.
 *
 * Starting at index `N=1` within the byte array `map[]`:
 *
 *   - Interpret `map[0]` as the length of the `map[]` array.  A zero
 *     array length is invalid and is treated as a fatal error.
 *
 *     - Upon reaching the end of the mapping (i.e. `N==map[0]`),
 *       stop processing.
 *
 *   - Interpret `map[N]` as the eight bits `llllllvv`, where:
 *
 *     - `V = vv` is the minimal TLS version that includes this field
 *       (encoded as a delta from the lowest version currently
 *       supported by the codebase).
 *
 *     - `L = llllll` is one plus the length of the corresponding
 *       fixed-length data structure.  An empty fixed-length data
 *       structure is pointless, and so we choose a zero fixed length
 *       (i.e. `L==1`) to represent a variable-length data structure.
 *       A negative fixed length (i.e. `L==0`) is invalid and is
 *       treated as a fatal error.
 *
 *   - For a fixed-length data structure (with `L>1`):
 *
 *     - When parsing: store the pointer to the start of the
 *       fixed-length data as `ptrlen[N-1]`.
 *
 *     - When building: append a copy of the data from `ptrlen[N-1]`
 *       with length `L-1`.
 *
 *   - For a variable-length data structure (with `L==1`), interpret
 *     `map[N+1]` as the eight bits `xxxxxxnn`, where:
 *
 *     - `N = nn` is the length of the length header that precedes the
 *       variable-length data structure.  A zero-byte length header is
 *       invalid, and so we choose `N==0` to represent a field that
 *       covers all remaining data.
 *
 *     - `X = xxxxxx` is one plus the number of TLS extension types
 *       that immediately follow `map[N+1]` and that represent
 *       extensions of interest that may be present within this field.
 *       A negative number of extensions (i.e. `X==0`) is impossible
 *       and so we choose this to represent a field that is not used
 *       to contain extensions.
 *
 *     - When parsing: store the pointer to the start of the
 *       variable-length data as `ptrlen[N-1]`, and store the decoded
 *       length of the variable-length data as `ptrlen[N]`.
 *
 *     - When building: append a copy of the data from `ptrlen[N-1]`
 *       (with the correct `N`-byte length header).  Take the length
 *       of the variable-length data to be `X ? 0 : ptrlen[N]`
 *       (i.e. always build fields that contain extensions as being
 *       empty).
 *
 *   - For each `1<=k<=(X-1)` in a variable-length data structure that
 *     is used to contain extensions (i.e. that has `X>0`), interpret
 *     `T = map[N+2k]:map[N+2k+1]` as the inverse of a TLS extension
 *     type (in network-endian order):
 *
 *     - If `T==0`, then terminate processing with a fatal error.
 *
 *     - When parsing: for each matching extension type `~T` that is
 *       found, store the pointer to the start of the extension data
 *       as `ptrlen[N+2k-1]`, and store the decoded length of the
 *       variable-length extension data as `ptrlen[N+2k]`.
 *
 *     - When building: if `ptrlen[N+2k-1]` is set, then append a copy
 *       of the data from `ptrlen[N+2k-1]` with length `ptrlen[N+2k]`
 *       as an extension with type `~T` (with the correct extension
 *       header, and updating the length of the containing field
 *       accordingly).
 *
 *   - Increment `N` to the next uninterpreted byte, and loop.
 *
 * Any parsing failure (e.g. insufficient remaining space to contain
 * the fixed-length or variable-length structure) will be treated as a
 * fatal error.  Missing extensions of interest will not be treated as
 * errors, and the corresponding `ptrlen` entries will be zeroed to
 * indicate that the extension was not found.  Duplicate extensions of
 * interest will be treated as a fatal error.  (Any extensions that
 * are not of interest will be ignored, even if duplicated.)
 *
 * The encoding allows for fixed-length fields of up to 62 bytes.
 * Longer fixed-length fields must be expressed as a sequence of
 * smaller fixed-length fields.
 *
 * All fields must be represented using data structures that are
 * packed and that allow for arbitrary byte alignment.
 *
 * A variable-length field that covers all remaining data (i.e. with
 * `N==0`) may be used for incremental parsing, as required for the
 * variable number of entries in structures such as a TLS extension
 * list.  If no such variable-length field exists, then any leftover
 * data will be treated as a fatal parsing error.
 *
 * Up to four consecutive versions of TLS may be supported by the
 * encoding.  Experience shows that at most three versions of TLS will
 * be supported by the codebase at any one time, and so this is likely
 * to be sufficient in practice.
 *
 * The encoding is designed to allow for efficient population of the
 * byte array using preprocessor macros (with compile-time checks to
 * ensure that the byte array matches the descriptor structure).  A
 * missing populator macro will leave an erroneous zero byte within
 * the byte array, and the encoding has been designed so that stray
 * zero bytes will fail safe: `map[0]==0` is treated as a fatal error,
 * `L==0` or `T==0` are treated as fatal errors, and `N==0` would
 * capture all remaining data (and so cause parsing to fail on the
 * subsequent byte).
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <stddef.h>
#include <ipxe/asn1.h>

/** TLS version 1.1 */
#define TLS_VERSION_TLS_1_1 0x0302

/** TLS version 1.2 */
#define TLS_VERSION_TLS_1_2 0x0303

/** TLS version 1.3 */
#define TLS_VERSION_TLS_1_3 0x0304

/** Lowest configurable supported version */
#define TLS_VERSION_BASE TLS_VERSION_TLS_1_1

/** A TLS variable-length data cursor */
struct tls_cursor {
	/** Data */
	void *data;
	/** Length of data */
	size_t len;
};

/** A pointer/length value */
union tls_ptr_len {
	/** Data pointer */
	void *data;
	/** Length */
	size_t len;
};

/**
 * Number of pointer/length values in a descriptor structure
 *
 * @v desc		Descriptor structure name
 * @ret count		Number of pointer/length values
 */
#define TLS_DESCR_COUNT( desc )						\
	( /* Calculate count */						\
	  ( sizeof ( struct desc ) / sizeof ( union tls_ptr_len ) ) +	\
	  /* Check alignment */						\
	  ( 0 * sizeof ( int[ -( sizeof ( struct desc ) %		\
				 sizeof ( union tls_ptr_len ) ) ] ) ) )

/**
 * Binary encoding of a descriptor structure mapping
 *
 * @v desc		Descriptor structure name
 * @ret map		Binary encoding of the descriptor structure mapping
 */
#define TLS_DESCR_MAPPING( desc )					\
	const uint8_t desc ## _map [ 1 /* Mapping size byte */ +	\
				     TLS_DESCR_COUNT ( desc ) ]

/**
 * Describe size of a descriptor mapping
 *
 * @v desc		Descriptor stucture name
 */
#define TLS_MAPSZ( desc )						\
	[0] = ( 1 /* Mapping size byte */ + TLS_DESCR_COUNT ( desc ) )

/**
 * Index of descriptor field
 *
 * @v desc		Descriptor structure name
 * @v field		Field name within descriptor structure
 * @ret index		Index within byte array or ptr/len array
 */
#define TLS_INDEX( desc, field )					\
	( /* Skip mapping size byte */					\
	  1 +								\
	  /* Index within ptr/len array */				\
	  ( offsetof ( struct desc, field ) /				\
	    sizeof ( union tls_ptr_len ) ) +				\
	  /* Check alignment */						\
	  ( 0 *	sizeof ( int[ -( offsetof ( struct desc, field ) %	\
				 sizeof ( union tls_ptr_len ) ) ] ) ) )

/**
 * Number of extensions of interest within an extension descriptor
 *
 * @v desc		Descriptor structure name
 * @v field		Field name within descriptor structure
 * @ret extns		Number of extensions of interest within this field
 */
#define TLS_EXTNS( desc, field )					\
	( ( sizeof ( ( ( struct desc * ) NULL )->field ) -		\
	    sizeof ( ( ( struct desc * ) NULL )->field.all ) )		\
	  / sizeof ( struct tls_cursor ) )

/**
 * Describe a fixed-length field
 *
 * @v desc		Descriptor structure name
 * @v version		Minimum TLS version that includes this field
 * @v field		Field name within descriptor structure
 */
#define TLS_FIXED( desc, version, field )				\
	/* Encoded `llllllvv` */					\
	[ TLS_INDEX ( desc, field ) ] =					\
		( ( (version) - TLS_VERSION_BASE ) +			\
		  ( ( sizeof ( *( ( ( struct desc * ) NULL )->field ) )	\
		      + 1 ) << 2 ) )

/**
 * Describe a variable-length field
 *
 * @v desc		Descriptor structure name
 * @v version		Minimum TLS version that includes this field
 * @v field		Field name within descriptor structure
 * @v bits		Number of bits used to encode field length
 * @v extns		Number of extensions of interest plus one (if any)
 */
#define TLS_VARIABLE( desc, version, field, bits, extns )		\
	/* Encoded `000001vv` */					\
	[ TLS_INDEX ( desc, field ) ] =					\
		( ( (version) - TLS_VERSION_BASE ) + ( 1 << 2 ) ),	\
	/* Encoded `xxxxxxnn` */					\
	[ TLS_INDEX ( desc, field ) + 1 ] =				\
		( ( ( (bits) + 7 ) / 8 ) + ( (extns) << 2 ) )

/**
 * Describe a variable-length field that captures all remaining data
 *
 * @v desc		Descriptor structure name
 * @v version		Minimum TLS version that includes this field
 * @v field		Field name within descriptor structure
 */
#define TLS_EXTRA( desc, version, field )				\
	TLS_VARIABLE ( desc, version, field, 0, 0 )

/**
 * Describe a variable-length field with an 8-bit length
 *
 * @v desc		Descriptor structure name
 * @v version		Minimum TLS version that includes this field
 * @v field		Field name within descriptor structure
 */
#define TLS_VAR08( desc, version, field )				\
	TLS_VARIABLE ( desc, (version), field, 8, 0 )

/**
 * Describe a variable-length field with a 16-bit length
 *
 * @v desc		Descriptor structure name
 * @v version		Minimum TLS version that includes this field
 * @v field		Field name within descriptor structure
 */
#define TLS_VAR16( desc, version, field )				\
	TLS_VARIABLE ( desc, (version), field, 16, 0 )

/**
 * Describe a variable-length field with a 24-bit length
 *
 * @v desc		Descriptor structure name
 * @v version		Minimum TLS version that includes this field
 * @v field		Field name within descriptor structure
 */
#define TLS_VAR24( desc, version, field )				\
	TLS_VARIABLE ( desc, (version), field, 24, 0 )

/**
 * Describe a variable-length field containing extensions
 *
 * @v desc		Descriptor structure name
 * @v version		Minimum TLS version that includes this field
 * @v field		Field name within descriptor structure
 */
#define TLS_EXT16( desc, version, field )				\
	TLS_VARIABLE ( desc, (version), field.all, 16,			\
		       ( TLS_EXTNS ( desc, field ) + 1 ) )

/**
 * Describe a variable-length field containing an extension of interest
 *
 * @v desc		Descriptor structure name
 * @v extension		Extension type
 * @v field		Field name within descriptor structure
 */
#define TLS_EXTND( desc, extension, field )				\
	[ TLS_INDEX ( desc, field ) ] =					\
		( ( (extension) >> 8 ) ^ 0xff ),			\
	[ TLS_INDEX ( desc, field ) + 1 ] =				\
		( ( (extension) & 0xff ) ^ 0xff )

/**
 * Interpret minimum version from an `llllllvv` byte
 *
 * @v byte		Mapping byte
 * @ret min		Minimum TLS version that includes this field
 */
#define TLS_MAP_MIN( byte ) ( TLS_VERSION_BASE + ( (byte) & 0x03 ) )

/**
 * Interpret fixed length from an `llllllvv` byte
 *
 * @v byte		Mapping byte
 * @ret fixed		Fixed length
 */
#define TLS_MAP_FIXED( byte ) ( ( (byte) >> 2 ) - 1 )

/**
 * Interpret number of length bytes from an `xxxxxxnn` byte
 *
 * @v byte		Mapping byte
 * @ret len_len		Number of length bytes
 */
#define TLS_MAP_LEN_LEN( byte ) ( (byte) & 0x03 )

/** Maximum number of length bytes */
#define TLS_MAP_LEN_LEN_MAX 3

/**
 * Interpret number of extensions from an `xxxxxxnn` byte
 *
 * @v byte		Mapping byte
 * @ret extensions	Number of extensions
 */
#define TLS_MAP_EXTENSIONS( byte ) ( ( (byte) >> 2 ) - 1 )

/** Certificate descriptor */
struct tls_certificate {
	/** Certificate request context */
	struct tls_cursor context;
	/** Certificate list */
	struct tls_cursor list;
};

/** CertificateEntry descriptor */
struct tls_certificate_entry {
	/** Certificate data */
	struct tls_cursor cert;
	/** Extensions of interest */
	struct {
		/** All extensions */
		struct tls_cursor all;
	} ext;
	/** Next certificate */
	struct tls_cursor next;
};

/** DigitallySigned descriptor */
struct tls_digitally_signed {
	/** Signature and hash algorithm */
	uint16_t __attribute__ (( aligned ( 1 ) )) *sig_hash;
	/** Signature */
	struct tls_cursor sig;
};

/** Extension descriptor */
struct tls_extension {
	/** Extension type */
	uint16_t __attribute__ (( aligned ( 1 ) )) *type;
	/** Extension data */
	struct tls_cursor data;
	/** Next extension */
	struct tls_cursor next;
};

/** HelloRequest descriptor */
struct tls_hello_request {};

/** KeyShareEntry descriptor */
struct tls_key_share_entry {
	/** Named group */
	uint16_t __attribute__ (( aligned ( 1 ) )) *group;
	/** Public key */
	struct tls_cursor public;
	/** Next key share */
	struct tls_cursor next;
};

/** NewSessionTicket descriptor */
struct tls_new_session_ticket {
	/** Lifetime hint */
	uint32_t __attribute__ (( aligned ( 1 ) )) *lifetime;
	/** Age obfuscation */
	uint32_t __attribute__ (( aligned ( 1 ) )) *age;
	/** Nonce */
	struct tls_cursor nonce;
	/** Ticket */
	struct tls_cursor ticket;
	/** Extensions of interest */
	struct {
		/** All extensions */
		struct tls_cursor all;
	} ext;
};

/** RenegotiationInfo descriptor */
struct tls_renegotiation_info {
	/** Verification data from previous Finished */
	struct tls_cursor verify;
};

/** ServerHello descriptor */
struct tls_server_hello {
	/** First fixed-length portion */
	struct {
		/** Selected version */
		uint16_t version;
		/** Server random bytes */
		uint8_t random[32];
	} __attribute__ (( packed )) *a;
	/** Session ID */
	struct tls_cursor session_id;
	/** Second fixed-length portion */
	struct {
		/** Selected cipher suite */
		uint16_t cipher_suite;
		/** Selected compression method */
		uint8_t compression_method;
	} __attribute__ (( packed )) *b;
	/** Extensions of interest */
	struct {
		/** All extensions */
		struct tls_cursor all;
		/** Renegotiation information extension */
		struct tls_cursor reneg;
		/** Extended master secret extension */
		struct tls_cursor ems;
		/** Supported version */
		struct tls_cursor supver;
		/** Key share */
		struct tls_cursor key;
	} ext;
};

/** ServerHelloDone descriptor */
struct tls_server_hello_done {};

/** ServerKeyExchange descriptor (for DHE) */
struct tls_server_key_exchange_dhe {
	/** Prime modulus */
	struct tls_cursor dh_p;
	/** Generator */
	struct tls_cursor dh_g;
	/** Public key */
	struct tls_cursor dh_ys;
	/** Signature  */
	struct tls_cursor dsig;
};

/** ServerKeyExchange descriptor (for ECDHE) */
struct tls_server_key_exchange_ecdhe {
	/** Curve parameters */
	struct {
		/** Curve type */
		uint8_t type;
		/** Named group */
		uint16_t group;
	} __attribute__ (( packed )) *curve;
	/** Curve point */
	struct tls_cursor point;
	/** Signature  */
	struct tls_cursor dsig;
};

/** SupportedVersions descriptor (in ServerHello) */
struct tls_supported_version {
	/** Selected version */
	uint16_t __attribute__ (( aligned ( 1 ) )) *selected;
};

/** SupportedVersions descriptor (in ClientHello) */
struct tls_supported_versions {
	/** Supported versions */
	struct tls_cursor versions;
};

/**
 * Get ASN.1 cursor from TLS cursor
 *
 * @v cursor		TLS cursor
 * @ret asn1		ASN.1 object cursor
 */
static inline __attribute__ (( always_inline )) const struct asn1_cursor *
tls_asn1 ( const struct tls_cursor *cursor ) {
	union {
		const struct tls_cursor tls;
		const struct asn1_cursor asn1;
	} *u = container_of ( cursor, typeof ( *u ), tls );

	/* Sanity check */
	build_assert ( ( ( const void * ) &u->tls.data ) == &u->asn1.data );
	build_assert ( &u->tls.len == &u->asn1.len );

	return &u->asn1;
}

extern int tls_parse_map ( const uint8_t *map, unsigned int version,
			   const struct tls_cursor *cursor,
			   union tls_ptr_len *desc );
extern int tls_parse_opt_map ( const uint8_t *map, unsigned int version,
			       const struct tls_cursor *cursor,
			       union tls_ptr_len *desc );
extern int tls_build_map ( const uint8_t *map, unsigned int version,
			   union tls_ptr_len *desc,
			   struct tls_cursor *cursor );

/**
 * Calculate length of TLS data structure
 *
 * @v type		Descriptor structure name
 * @v version		Protocol version
 * @v desc		Data structure descriptor to fill in
 * @v cursor		Cursor to contain TLS data structure
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
tls_size_map ( const uint8_t *map, unsigned int version,
	       union tls_ptr_len *desc, struct tls_cursor *cursor ) {

	cursor->data = NULL;
	return tls_build_map ( map, version, desc, cursor );
}

/**
 * Parse TLS data structure
 *
 * @v type		Descriptor structure name
 * @v version		Protocol version
 * @v cursor		Cursor containing TLS data structure
 * @v desc		Data structure descriptor to fill in
 * @ret rc		Return status code
 */
#define tls_parse( type, version, cursor, desc )			\
	tls_parse_map ( type ## _map, (version), (cursor),		\
			( ( union tls_ptr_len * )			\
			  ( (desc) == ( ( struct type * ) NULL ) ?	\
			    (desc) : (desc) ) ) )

/**
 * Parse optional TLS data structure
 *
 * @v type		Descriptor structure name
 * @v version		Protocol version
 * @v cursor		Cursor containing TLS data structure
 * @v desc		Data structure descriptor to fill in
 * @ret rc		Return status code
 */
#define tls_parse_opt( type, version, cursor, desc )			\
	tls_parse_opt_map ( type ## _map, (version), (cursor),		\
			    ( ( union tls_ptr_len * )			\
			      ( (desc) == ( ( struct type * ) NULL ) ?	\
				(desc) : (desc) ) ) )

/**
 * Build TLS data structure
 *
 * @v type		Descriptor structure name
 * @v version		Protocol version
 * @v desc		Data structure descriptor to fill in
 * @v cursor		Cursor to contain TLS data structure
 * @ret rc		Return status code
 */
#define tls_build( type, version, desc, cursor )			\
	tls_build_map ( type ## _map, (version),			\
			( ( union tls_ptr_len * )			\
			  ( (desc) == ( ( struct type * ) NULL ) ?	\
			    (desc) : (desc) ) ), (cursor) )

/**
 * Calculate length of TLS data structure
 *
 * @v type		Descriptor structure name
 * @v version		Protocol version
 * @v desc		Data structure descriptor to fill in
 * @v cursor		Cursor to contain TLS data structure
 * @ret rc		Return status code
 */
#define tls_size( type, version, desc, cursor )				\
	tls_size_map ( type ## _map, (version),				\
		       ( ( union tls_ptr_len * )			\
			 ( (desc) == ( ( struct type * ) NULL ) ?	\
			   (desc) : (desc) ) ), (cursor) )

extern TLS_DESCR_MAPPING ( tls_certificate );
extern TLS_DESCR_MAPPING ( tls_certificate_entry );
extern TLS_DESCR_MAPPING ( tls_digitally_signed );
extern TLS_DESCR_MAPPING ( tls_extension );
extern TLS_DESCR_MAPPING ( tls_hello_request );
extern TLS_DESCR_MAPPING ( tls_key_share_entry );
extern TLS_DESCR_MAPPING ( tls_new_session_ticket );
extern TLS_DESCR_MAPPING ( tls_renegotiation_info );
extern TLS_DESCR_MAPPING ( tls_server_hello );
extern TLS_DESCR_MAPPING ( tls_server_hello_done );
extern TLS_DESCR_MAPPING ( tls_server_key_exchange_dhe );
extern TLS_DESCR_MAPPING ( tls_server_key_exchange_ecdhe );
extern TLS_DESCR_MAPPING ( tls_supported_version );
extern TLS_DESCR_MAPPING ( tls_supported_versions );

#endif /* _IPXE_TLSFMT_H */
