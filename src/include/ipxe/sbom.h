#ifndef _IPXE_SBOM_H
#define _IPXE_SBOM_H

/** @file
 *
 * Software Bill Of Materials (SBOM)
 *
 * Since October 2025, the Microsoft UEFI Signing Requirements have
 * included a clause stating that "submissions must contain a valid
 * signed SPDX SBOM in a custom '.sbom' PE section".  A list of
 * required fields is provided, and a link is given to "the Microsoft
 * SBOM tool to aid SBOM generation".  So far, so promising.
 *
 * The Microsoft SBOM tool has no support for handling a .sbom PE
 * section.  There is no published document that specifies what is
 * supposed to appear within this PE section.  An educated guess is
 * that it should probably contain the raw JSON data in the same
 * format that the Microsoft SBOM tool produces.
 *
 * The list of required fields does not map to identifiable fields
 * within the JSON.  In particular:
 *
 * - "file name / software"
 *
 *   This might be the top-level "name" field.  It's hard to tell.
 *   The SPDX SBOM specification is not particularly informative
 *   either: the only definition it appears to give for "name" is
 *   "This field identifies the name of an Element as designated by
 *   the creator", which is a spectacularly useless definition.
 *
 * - "software version / component generation (shim)"
 *
 *   This may refer to the "packages[].versionInfo" field.  There is
 *   no obvious relevance for the words "component", "generation", or
 *   "shim".  The proximity of "generation" and "shim" suggests that
 *   this might be related in some way to the SBAT security
 *   generation, which is absolutely not the same thing as the
 *   software version.
 *
 * - "vendor / company name (this must exactly match the verified
 *   company name in the submitter's EV certificate on the Microsoft
 *   HDC partner center account)"
 *
 *   This is clearly written as though it has some significance for
 *   the UEFI signing submission process.  Unfortunately there is no
 *   obvious map to any defined SBOM field.  An educated guess is that
 *   this might be referring to "packages[].supplier", since
 *   experiments show that the Microsoft SBOM tool will fail
 *   validation unless this field is present.
 *
 * - "product-name"
 *
 *   This might also be the top-level "name" field.  There is no
 *   indication given as to how this might differ from "file name /
 *   software".
 *
 * - "OEM Name" and "OEM ID"
 *
 *   These seem to be terms made up on the spur of the moment.  The
 *   three-letter sequence "OEM" does not appear anywhere within the
 *   codebase of the Microsoft SBOM tool.
 *
 * In the absence of any meaningful specification, we choose not to
 * engage in good faith with this requirement.  Instead, we construct
 * a best guess at the contents of a .sbom section that has some
 * chance of being accepted by the UEFI signing submission process.
 * We assume that anything that passes "sbom-tool validate" will
 * probably be accepted, with the only actual check being that the
 * supplier name must match the registered EV code signing
 * certificate.
 *
 * To anyone who actually cares about the arguably valuable benefits
 * of having a software bill of materials: please stop creating junk
 * requirements.  If you want people to actually make the effort to
 * produce useful SBOM data, then make it clear what data you want.
 * Provide unambiguous specifications.  Provide example files.
 * Provide tools that actually do the job they are claimed to do.
 * Don't just throw out another piece of "MUST HAS THING BECAUSE IS
 * MORE SECURITY" garbage and call it a day.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/** An SBOM field */
#define SBOM_FIELD( key, value ) "\"" key "\":" value

/** An SBOM string field */
#define SBOM_STRING( key, value ) SBOM_FIELD ( key, "\"" value "\"" )

/** An SBOM package */
#define SBOM_PACKAGE( spdxid, name, supplier, version )			\
	"{"								\
	SBOM_STRING ( "SPDXID", spdxid ) ","				\
	SBOM_STRING ( "name", name ) ","				\
	SBOM_STRING ( "supplier", supplier ) ","			\
	SBOM_STRING ( "versionInfo", version )				\
	"}"

/** An SBOM manifest */
#define SBOM_MANIFEST( name, supplier, version )			\
	"{"								\
	SBOM_STRING ( "name", name ) ","				\
	SBOM_FIELD ( "files", "[]" ) ","				\
	SBOM_FIELD ( "relationships", "[]" ) ","			\
	SBOM_FIELD ( "packages", "["					\
		     SBOM_PACKAGE ( "", name, supplier, version )	\
		     "]" )						\
	"}"

/** Mark variable as being in the ".sbom" section */
#define __sbom __attribute__ (( section ( ".sbom" ), aligned ( 512 ) ))

extern const char sbom[] __sbom;

#endif /* _IPXE_SBOM_H */
