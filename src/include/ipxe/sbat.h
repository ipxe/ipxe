#ifndef _IPXE_SBAT_H
#define _IPXE_SBAT_H

/** @file
 *
 * Secure Boot Advanced Targeting (SBAT)
 *
 * SBAT defines an encoding for security generation numbers stored as
 * a CSV file within a special ".sbat" section in the signed binary.
 * If a Secure Boot exploit is discovered then the generation number
 * will be incremented alongside the corresponding fix.
 *
 * Platforms may then record the minimum generation number required
 * for any given product.  This allows for an efficient revocation
 * mechanism that consumes minimal flash storage space (in contrast to
 * the DBX mechanism, which allows for only a single-digit number of
 * revocation events to ever take place across all possible signed
 * binaries).
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * A single line within an SBAT CSV file
 *
 * @v name		Machine-readable component name
 * @v generation	Security generation number
 * @v vendor		Human-readable vendor name
 * @v package		Human-readable package name
 * @v version		Human-readable package version
 * @v uri		Contact URI
 * @ret line		CSV line
 */
#define SBAT_LINE( name, generation, vendor, package, version, uri )	\
	name "," _S2 ( generation ) "," vendor "," package ","		\
	version "," uri "\n"

/** SBAT format generation */
#define SBAT_GENERATION 1

/** Upstream security generation
 *
 * This represents the security generation of the upstream codebase.
 * It will be incremented whenever a Secure Boot exploit is fixed in
 * the upstream codebase.
 *
 * If you do not have commit access to the upstream iPXE repository,
 * then you may not modify this value under any circumstances.
 */
#define IPXE_SBAT_GENERATION 1

/* Seriously, do not modify this value */
#if IPXE_SBAT_GENERATION != 1
#error "You may not modify IPXE_SBAT_GENERATION"
#endif

/** SBAT header line */
#define SBAT_HEADER							\
	SBAT_LINE ( "sbat", SBAT_GENERATION, "SBAT Version", "sbat",	\
		    _S2 ( SBAT_GENERATION ),				\
		    "https://github.com/rhboot/shim/blob/main/SBAT.md" )

/** Mark variable as being in the ".sbat" section */
#define __sbat __attribute__ (( section ( ".sbat" ), aligned ( 512 ) ))

extern const char sbat[] __sbat;

#endif /* _IPXE_SBAT_H */
