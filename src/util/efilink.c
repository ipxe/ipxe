#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <bfd.h>

struct bfd_file {
	bfd *bfd;
	asymbol **symtab;
	long symcount;
};

struct pe_relocs {
	struct pe_relocs *next;
	unsigned long start_rva;
	unsigned int used_relocs;
	unsigned int total_relocs;
	uint16_t *relocs;
};

/**
 * Allocate memory
 *
 * @v len		Length of memory to allocate
 * @ret ptr		Pointer to allocated memory
 */
static void * xmalloc ( size_t len ) {
	void *ptr;

	ptr = malloc ( len );
	if ( ! ptr ) {
		fprintf ( stderr, "Could not allocate %zd bytes\n", len );
		exit ( 1 );
	}

	return ptr;
}

/**
 * Generate entry in PE relocation table
 *
 * @v pe_reltab		PE relocation table
 * @v rva		RVA
 * @v size		Size of relocation entry
 */
static void generate_pe_reloc ( struct pe_relocs **pe_reltab,
				unsigned long rva, size_t size ) {
	unsigned long start_rva;
	uint16_t reloc;
	struct pe_relocs *pe_rel;
	uint16_t *relocs;

	/* Construct */
	start_rva = ( rva & ~0xfff );
	reloc = ( rva & 0xfff );
	switch ( size ) {
	case 4:
		reloc |= 0x3000;
		break;
	case 2:
		reloc |= 0x2000;
		break;
	default:
		fprintf ( stderr, "Unsupported relocation size %zd\n", size );
		exit ( 1 );
	}

	/* Locate or create PE relocation table */
	for ( pe_rel = *pe_reltab ; pe_rel ; pe_rel = pe_rel->next ) {
		if ( pe_rel->start_rva == start_rva )
			break;
	}
	if ( ! pe_rel ) {
		pe_rel = xmalloc ( sizeof ( *pe_rel ) );
		memset ( pe_rel, 0, sizeof ( *pe_rel ) );
		pe_rel->next = *pe_reltab;
		*pe_reltab = pe_rel;
		pe_rel->start_rva = start_rva;
	}

	/* Expand relocation list if necessary */
	if ( pe_rel->used_relocs < pe_rel->total_relocs ) {
		relocs = pe_rel->relocs;
	} else {
		pe_rel->total_relocs = ( pe_rel->total_relocs ?
					 ( pe_rel->total_relocs * 2 ) : 256 );
		relocs = xmalloc ( pe_rel->total_relocs *
				   sizeof ( pe_rel->relocs[0] ) );
		memset ( relocs, 0,
			 pe_rel->total_relocs * sizeof ( pe_rel->relocs[0] ) );
		memcpy ( relocs, pe_rel->relocs,
			 pe_rel->used_relocs * sizeof ( pe_rel->relocs[0] ) );
		free ( pe_rel->relocs );
		pe_rel->relocs = relocs;
	}

	/* Store relocation */
	pe_rel->relocs[ pe_rel->used_relocs++ ] = reloc;
}

/**
 * Calculate size of binary PE relocation table
 *
 * @v pe_reltab		PE relocation table
 * @v buffer		Buffer to contain binary table, or NULL
 * @ret size		Size of binary table
 */
static size_t output_pe_reltab ( struct pe_relocs *pe_reltab,
				 void *buffer ) {
	struct pe_relocs *pe_rel;
	unsigned int num_relocs;
	size_t size;
	size_t total_size = 0;

	for ( pe_rel = pe_reltab ; pe_rel ; pe_rel = pe_rel->next ) {
		num_relocs = ( ( pe_rel->used_relocs + 1 ) & ~1 );
		size = ( sizeof ( uint32_t ) /* VirtualAddress */ +
			 sizeof ( uint32_t ) /* SizeOfBlock */ +
			 ( num_relocs * sizeof ( uint16_t ) ) );
		if ( buffer ) {
			*( (uint32_t *) ( buffer + total_size + 0 ) )
				= pe_rel->start_rva;
			*( (uint32_t *) ( buffer + total_size + 4 ) ) = size;
			memcpy ( ( buffer + total_size + 8 ), pe_rel->relocs,
				 ( num_relocs * sizeof ( uint16_t ) ) );
		}
		total_size += size;
	}

	return total_size;
}

/**
 * Read symbol table
 *
 * @v bfd		BFD file
 */
static void read_symtab ( struct bfd_file *bfd ) {
	long symtab_size;

	/* Get symbol table size */
	symtab_size = bfd_get_symtab_upper_bound ( bfd->bfd );
	if ( symtab_size < 0 ) {
		bfd_perror ( "Could not get symbol table upper bound" );
		exit ( 1 );
	}

	/* Allocate and read symbol table */
	bfd->symtab = xmalloc ( symtab_size );
	bfd->symcount = bfd_canonicalize_symtab ( bfd->bfd, bfd->symtab );
	if ( bfd->symcount < 0 ) {
		bfd_perror ( "Cannot read symbol table" );
		exit ( 1 );
	}
}

/**
 * Read relocation table
 *
 * @v bfd		BFD file
 * @v section		Section
 * @v symtab		Symbol table
 * @ret reltab		Relocation table
 */
static arelent ** read_reltab ( struct bfd_file *bfd, asection *section ) {
	long reltab_size;
	arelent **reltab;
	long numrels;

	/* Get relocation table size */
	reltab_size = bfd_get_reloc_upper_bound ( bfd->bfd, section );
	if ( reltab_size < 0 ) {
		bfd_perror ( "Could not get relocation table upper bound" );
		exit ( 1 );
	}

	/* Allocate and read relocation table */
	reltab = xmalloc ( reltab_size );
	numrels = bfd_canonicalize_reloc ( bfd->bfd, section, reltab,
					   bfd->symtab );
	if ( numrels < 0 ) {
		bfd_perror ( "Cannot read relocation table" );
		exit ( 1 );
	}

	return reltab;
}


/**
 * Open input BFD file
 *
 * @v filename		File name
 * @ret ibfd		BFD file
 */
static struct bfd_file * open_input_bfd ( const char *filename ) {
	struct bfd_file *ibfd;

	/* Create BFD file */
	ibfd = xmalloc ( sizeof ( *ibfd ) );
	memset ( ibfd, 0, sizeof ( *ibfd ) );

	/* Open the file */
	ibfd->bfd = bfd_openr ( filename, NULL );
	if ( ! ibfd->bfd ) {
		fprintf ( stderr, "Cannot open %s: ", filename );
		bfd_perror ( NULL );
		exit ( 1 );
	}

	/* The call to bfd_check_format() must be present, otherwise
	 * we get a segfault from later BFD calls.
	 */
	if ( bfd_check_format ( ibfd->bfd, bfd_object ) < 0 ) {
		fprintf ( stderr, "%s is not an object file\n", filename );
		exit ( 1 );
	}

	/* Read symbols and relocation entries */
	read_symtab ( ibfd );

	return ibfd;
}

/**
 * Open output BFD file
 *
 * @v filename		File name
 * @v ibfd		Input BFD file
 * @ret obfd		BFD file
 */
static struct bfd_file * open_output_bfd ( const char *filename,
					   struct bfd_file *ibfd ) {
	struct bfd_file *obfd;
	asection *isection;
	asection *osection;

	/*
	 * Most of this code is based on what objcopy.c does.
	 *
	 */

	/* Create BFD file */
	obfd = xmalloc ( sizeof ( *obfd ) );
	memset ( obfd, 0, sizeof ( *obfd ) );

	/* Open the file */
	obfd->bfd = bfd_openw ( filename, ibfd->bfd->xvec->name );
	if ( ! obfd->bfd ) {
		fprintf ( stderr, "Cannot open %s: ", filename );
		bfd_perror ( NULL );
		exit ( 1 );
	}

	/* Copy per-file data */
	if ( ! bfd_set_arch_mach ( obfd->bfd, bfd_get_arch ( ibfd->bfd ),
				   bfd_get_mach ( ibfd->bfd ) ) ) {
		bfd_perror ( "Cannot copy architecture" );
		exit ( 1 );
	}
	if ( ! bfd_set_format ( obfd->bfd, bfd_get_format ( ibfd->bfd ) ) ) {
		bfd_perror ( "Cannot copy format" );
		exit ( 1 );
	}
	if ( ! bfd_copy_private_header_data ( ibfd->bfd, obfd->bfd ) ) {
		bfd_perror ( "Cannot copy private header data" );
		exit ( 1 );
	}

	/* Create sections */
	for ( isection = ibfd->bfd->sections ; isection ;
	      isection = isection->next ) {
		osection = bfd_make_section_anyway ( obfd->bfd,
						     isection->name );
		if ( ! osection ) {
			bfd_perror ( "Cannot create section" );
			exit ( 1 );
		}
		if ( ! bfd_set_section_flags ( obfd->bfd, osection,
					       isection->flags ) ) {
			bfd_perror ( "Cannot copy section flags" );
			exit ( 1 );
		}
		if ( ! bfd_set_section_size ( obfd->bfd, osection,
				 bfd_section_size ( ibfd->bfd, isection ) ) ) {
			bfd_perror ( "Cannot copy section size" );
			exit ( 1 );
		}
		if ( ! bfd_set_section_vma ( obfd->bfd, osection,
				  bfd_section_vma ( ibfd->bfd, isection ) ) ) {
			bfd_perror ( "Cannot copy section VMA" );
			exit ( 1 );
		}
		osection->lma = bfd_section_lma ( ibfd->bfd, isection );
		if ( ! bfd_set_section_alignment ( obfd->bfd, osection,
			    bfd_section_alignment ( ibfd->bfd, isection ) ) ) {
			bfd_perror ( "Cannot copy section alignment" );
			exit ( 1 );
		}
		osection->entsize = isection->entsize;
		isection->output_section = osection;
		isection->output_offset = 0;
		if ( ! bfd_copy_private_section_data ( ibfd->bfd, isection,
						       obfd->bfd, osection ) ){
			bfd_perror ( "Cannot copy section private data" );
			exit ( 1 );
		}
	}

	/* Copy symbol table */
	bfd_set_symtab ( obfd->bfd, ibfd->symtab, ibfd->symcount );
	obfd->symtab = ibfd->symtab;

	return obfd;
}

/**
 * Copy section from input BFD file to output BFD file
 *
 * @v obfd		Output BFD file
 * @v ibfd		Input BFD file
 * @v section		Section
 */
static void copy_bfd_section ( struct bfd_file *obfd, struct bfd_file *ibfd,
			       asection *isection ) {
	size_t size;
	void *buf;
	arelent **reltab;
	arelent **rel;
	char *errmsg;

	/* Read in original section */
	size = bfd_section_size ( ibfd->bfd, isection );
	if ( ! size )
		return;
	buf = xmalloc ( size );
	if ( ( ! bfd_get_section_contents ( ibfd->bfd, isection,
					    buf, 0, size ) ) ) {
		fprintf ( stderr, "Cannot read section %s: ", isection->name );
		bfd_perror ( NULL );
		exit ( 1 );
	}

	/* Perform relocations.  We do this here, rather than letting
	 * ld do it for us when creating the input ELF file, so that
	 * we can change symbol values as a result of having created
	 * the .reloc section.
	 */
	reltab = read_reltab ( ibfd, isection );
	for ( rel = reltab ; *rel ; rel++ ) {
		bfd_perform_relocation ( ibfd->bfd, *rel, buf, isection,
					 NULL, &errmsg );
	}
	free ( reltab );

	/* Write out modified section */
	if ( ( ! bfd_set_section_contents ( obfd->bfd,
					    isection->output_section,
					    buf, 0, size ) ) ) {
		fprintf ( stderr, "Cannot write section %s: ",
			  isection->output_section->name );
		bfd_perror ( NULL );
		exit ( 1 );
	}

	free ( buf );
}

/**
 * Process relocation record
 *
 * @v section		Section
 * @v rel		Relocation entry
 * @v pe_reltab		PE relocation table to fill in
 */
static void process_reloc ( asection *section, arelent *rel,
			    struct pe_relocs **pe_reltab ) {
	reloc_howto_type *howto = rel->howto;
	asymbol *sym = *(rel->sym_ptr_ptr);
	unsigned long offset = ( section->lma + rel->address );

	if ( bfd_is_abs_section ( sym->section ) ) {
		/* Skip absolute symbols; the symbol value won't
		 * change when the object is loaded.
		 */
	} else if ( strcmp ( howto->name, "R_386_32" ) == 0 ) {
		/* Generate a 4-byte PE relocation */
		generate_pe_reloc ( pe_reltab, offset, 4 );
	} else if ( strcmp ( howto->name, "R_386_16" ) == 0 ) {
		/* Generate a 2-byte PE relocation */
		generate_pe_reloc ( pe_reltab, offset, 2 );
	} else if ( strcmp ( howto->name, "R_386_PC32" ) == 0 ) {
		/* Skip PC-relative relocations; all relative offsets
		 * remain unaltered when the object is loaded.
		 */
	} else {
		fprintf ( stderr, "Unrecognised relocation type %s\n",
			  howto->name );
		exit ( 1 );
	}
}

/**
 * Create .reloc section
 *
 * obfd			Output BFD file
 * section		.reloc section in output file
 * pe_reltab		PE relocation table
 */
static void create_reloc_section ( struct bfd_file *obfd, asection *section,
				   struct pe_relocs *pe_reltab ) {
	size_t raw_size;
	size_t size;
	size_t old_size;
	void *buf;
	asymbol **sym;

	/* Build binary PE relocation table */
	raw_size = output_pe_reltab ( pe_reltab, NULL );
	size = ( ( raw_size + 31 ) & ~31 );
	buf = xmalloc ( size );
	memset ( buf, 0, size );
	output_pe_reltab ( pe_reltab, buf );

	/* Write .reloc section */
	old_size = bfd_section_size ( obfd->bfd, section );
	if ( ! bfd_set_section_size ( obfd->bfd, section, size ) ) {
		bfd_perror ( "Cannot resize .reloc section" );
		exit ( 1 );
	}
	if ( ! bfd_set_section_contents ( obfd->bfd, section,
					  buf, 0, size ) ) {
		bfd_perror ( "Cannot set .reloc section contents" );
		exit ( 1 );
	}

	/* Update symbols pertaining to the relocation directory */
	for ( sym = obfd->symtab ; *sym ; sym++ ) {
		if ( strcmp ( (*sym)->name, "_reloc_memsz" ) == 0 ) {
			(*sym)->value = size;
		} else if ( strcmp ( (*sym)->name, "_reloc_filesz" ) == 0 ){
			(*sym)->value = raw_size;
		} else if ( strcmp ( (*sym)->name, "_filesz" ) == 0 ) {
			(*sym)->value += ( size - old_size );
		}
	}
}

int main ( int argc, const char *argv[] ) {
	const char *iname;
	const char *oname;
	struct bfd_file *ibfd;
	struct bfd_file *obfd;
	asection *section;
	arelent **reltab;
	arelent **rel;
	struct pe_relocs *pe_reltab = NULL;
	asection *reloc_section;

	/* Initialise libbfd */
	bfd_init();

	/* Identify intput and output files */
	if ( argc != 3 ) {
		fprintf ( stderr, "Syntax: %s infile outfile\n", argv[0] );
		exit ( 1 );
	}
	iname = argv[1];
	oname = argv[2];

	/* Open BFD files */
	ibfd = open_input_bfd ( iname );
	obfd = open_output_bfd ( oname, ibfd );

	/* Process relocations in all sections */
	for ( section = ibfd->bfd->sections ; section ;
	      section = section->next ) {
		reltab = read_reltab ( ibfd, section );
		for ( rel = reltab ; *rel ; rel++ ) {
			process_reloc ( section, *rel, &pe_reltab );
		}
		free ( reltab );
	}

	/* Create modified .reloc section */
	reloc_section = bfd_get_section_by_name ( obfd->bfd, ".reloc" );
	if ( ! reloc_section ) {
		fprintf ( stderr, "Cannot find .reloc section\n" );
		exit ( 1 );
	}
	create_reloc_section ( obfd, reloc_section, pe_reltab );

	/* Copy other section contents */
	for ( section = ibfd->bfd->sections ; section ;
	      section = section->next ) {
		if ( section->output_section != reloc_section )
			copy_bfd_section ( obfd, ibfd, section );
	}

	/* Write out files and clean up */
	bfd_close ( obfd->bfd );
	bfd_close ( ibfd->bfd );

	return 0;
}
