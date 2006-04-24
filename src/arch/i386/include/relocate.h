#ifndef RELOCATE_H
#define RELOCATE_H

/* relocate() is conceptually impossible with KEEP_IT_REAL */
#ifndef KEEP_IT_REAL

#include <gpxe/tables.h>

/* An entry in the post-relocation function table */
struct post_reloc_fn {
	void ( *post_reloc ) ( void );
};

/* Use double digits to avoid problems with "10" < "9" on alphabetic sort */
#define POST_RELOC_LIBRM	00

/* Macro for creating a post-relocation function table entry */
#define POST_RELOC_FN( order, post_reloc_func )			\
	struct post_reloc_fn PREFIX_OBJECT(post_reloc_fn__)	\
	    __table ( post_reloc_fn, order ) = {		\
		.post_reloc = post_reloc_func,			\
	};

#endif

#endif /* RELOCATE_H */
