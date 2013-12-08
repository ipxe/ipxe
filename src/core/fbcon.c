/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Frame buffer console
 *
 */

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/ansiesc.h>
#include <ipxe/image.h>
#include <ipxe/pixbuf.h>
#include <ipxe/umalloc.h>
#include <ipxe/console.h>
#include <ipxe/fbcon.h>

/**
 * Calculate raw colour value
 *
 * @v fbcon		Frame buffer console
 * @v rgb		24-bit RGB value
 * @ret raw		Raw colour
 */
static uint32_t fbcon_colour ( struct fbcon *fbcon, uint32_t rgb ) {
	struct fbcon_colour_map *map = fbcon->map;
	uint8_t red = ( rgb >> 16 );
	uint8_t green = ( rgb >> 8 );
	uint8_t blue = ( rgb >> 0 );
	uint32_t mapped;

	mapped = ( ( ( red >> map->red_scale ) << map->red_lsb ) |
		   ( ( green >> map->green_scale ) << map->green_lsb ) |
		   ( ( blue >> map->blue_scale ) << map->blue_lsb ) );
	return cpu_to_le32 ( mapped );
}

/**
 * Calculate ANSI font colour
 *
 * @v fbcon		Frame buffer console
 * @v ansicol		ANSI colour value (0-based)
 * @ret colour		Raw colour
 */
static uint32_t fbcon_ansi_colour ( struct fbcon *fbcon,
				    unsigned int ansicol ) {
	uint32_t rgb;

	/* Treat ansicol as 3-bit BGR with intensity 0xaa */
	rgb = ( ( ( ansicol & ( 1 << 0 ) ) ? 0xaa0000 : 0 ) |
		( ( ansicol & ( 1 << 1 ) ) ? 0x00aa00 : 0 ) |
		( ( ansicol & ( 1 << 2 ) ) ? 0x0000aa : 0 ) );

	return fbcon_colour ( fbcon, rgb );
}

/**
 * Set default foreground colour
 *
 * @v fbcon		Frame buffer console
 */
static void fbcon_set_default_foreground ( struct fbcon *fbcon ) {

	/* Default to non-bold white foreground */
	fbcon->foreground = fbcon_ansi_colour ( fbcon, 0x7 );
	fbcon->bold = 0;
}

/**
 * Set default background colour
 *
 * @v fbcon		Frame buffer console
 */
static void fbcon_set_default_background ( struct fbcon *fbcon ) {

	/* Default to transparent background */
	fbcon->background = FBCON_TRANSPARENT;
}

/**
 * Store character at specified position
 *
 * @v fbcon		Frame buffer console
 * @v cell		Text cell
 * @v xpos		X position
 * @v ypos		Y position
 */
static void fbcon_store_character ( struct fbcon *fbcon,
				    struct fbcon_text_cell *cell,
				    unsigned int xpos, unsigned int ypos ) {
	size_t offset;

	/* Store cell */
	offset = ( ( ( ypos * fbcon->character.width ) + xpos ) *
		   sizeof ( *cell ) );
	copy_to_user ( fbcon->text.start, offset, cell, sizeof ( *cell ) );
}

/**
 * Draw character at specified position
 *
 * @v fbcon		Frame buffer console
 * @v cell		Text cell
 * @v xpos		X position
 * @v ypos		Y position
 */
static void fbcon_draw_character ( struct fbcon *fbcon,
				   struct fbcon_text_cell *cell,
				   unsigned int xpos, unsigned int ypos ) {
	static uint32_t black[FBCON_CHAR_WIDTH];
	struct fbcon_font_glyph glyph;
	userptr_t picture_start;
	size_t picture_offset;
	size_t picture_stride;
	size_t offset;
	size_t pixel_len;
	size_t skip_len;
	unsigned int row;
	unsigned int column;
	uint8_t bitmask;
	int transparent;
	void *src;

	/* Get font character */
	copy_from_user ( &glyph, fbcon->font->start,
			 ( cell->character * sizeof ( glyph ) ),
			 sizeof ( glyph ) );

	/* Calculate pixel geometry */
	offset = ( fbcon->indent +
		   ( ypos * fbcon->character.stride ) +
		   ( xpos * fbcon->character.len ) );
	pixel_len = fbcon->pixel->len;
	skip_len = ( fbcon->pixel->stride - fbcon->character.len );

	/* Calculate background picture geometry */
	if ( ( xpos < fbcon->picture.character.width ) &&
	     ( ypos < fbcon->picture.character.height ) ) {
		picture_start = fbcon->picture.start;
		picture_offset = ( fbcon->picture.indent +
				   ( ypos * fbcon->picture.character.stride ) +
				   ( xpos * fbcon->picture.character.len ) );
		picture_stride = fbcon->picture.pixel.stride;
	} else {
		picture_start = virt_to_user ( black );
		picture_offset = 0;
		picture_stride = 0;
	}
	assert ( fbcon->character.len <= sizeof ( black ) );

	/* Check for transparent background colour */
	transparent = ( cell->background == FBCON_TRANSPARENT );

	/* Draw character rows */
	for ( row = 0 ; row < FBCON_CHAR_HEIGHT ; row++ ) {

		/* Draw background picture */
		memcpy_user ( fbcon->start, offset, picture_start,
			      picture_offset, fbcon->character.len );

		/* Draw character row */
		for ( column = FBCON_CHAR_WIDTH, bitmask = glyph.bitmask[row] ;
		      column ; column--, bitmask <<= 1, offset += pixel_len ) {
			if ( bitmask & 0x80 ) {
				src = &cell->foreground;
			} else if ( ! transparent ) {
				src = &cell->background;
			} else {
				continue;
			}
			copy_to_user ( fbcon->start, offset, src, pixel_len );
		}

		/* Move to next row */
		offset += skip_len;
		picture_offset += picture_stride;
	}
}

/**
 * Redraw margins
 *
 * @v fbcon		Frame buffer console
 */
static void fbcon_redraw_margins ( struct fbcon *fbcon ) {
	struct fbcon_picture *picture = &fbcon->picture;
	size_t pixel_len = fbcon->pixel->len;
	size_t offset = 0;
	size_t picture_offset = 0;
	size_t row_len;
	size_t left_len;
	size_t right_len;
	size_t right_offset;
	unsigned int y;

	/* Calculate margin parameters */
	row_len = ( picture->pixel.width * pixel_len );
	left_len = ( picture->margin.left * pixel_len );
	right_offset = ( picture->margin.right * pixel_len );
	right_len = ( ( picture->pixel.width - picture->margin.right ) *
		      pixel_len );

	/* Redraw margins */
	for ( y = 0 ; y < picture->pixel.height ; y++ ) {
		if ( ( y < picture->margin.top ) ||
		     ( y >= picture->margin.bottom ) ) {

			/* Within top or bottom margin: draw whole row */
			memcpy_user ( fbcon->start, offset, picture->start,
				      picture_offset, row_len );

		} else {

			/* Otherwise, draw left and right margins */
			memcpy_user ( fbcon->start, offset, picture->start,
				      picture_offset, left_len );
			memcpy_user ( fbcon->start, ( offset + right_offset ),
				      picture->start,
				      ( picture_offset + right_offset ),
				      right_len );
		}
		offset += fbcon->pixel->stride;
		picture_offset += picture->pixel.stride;
	}
}

/**
 * Redraw characters
 *
 * @v fbcon		Frame buffer console
 */
static void fbcon_redraw_characters ( struct fbcon *fbcon ) {
	struct fbcon_text_cell cell;
	size_t offset = 0;
	unsigned int xpos;
	unsigned int ypos;

	/* Redraw characters */
	for ( ypos = 0 ; ypos < fbcon->character.height ; ypos++ ) {
		for ( xpos = 0 ; xpos < fbcon->character.width ; xpos++ ) {
			copy_from_user ( &cell, fbcon->text.start, offset,
					 sizeof ( cell ) );
			fbcon_draw_character ( fbcon, &cell, xpos, ypos );
			offset += sizeof ( cell );
		}
	}
}

/**
 * Redraw screen
 *
 * @v fbcon		Frame buffer console
 */
static void fbcon_redraw ( struct fbcon *fbcon ) {

	/* Redraw margins */
	fbcon_redraw_margins ( fbcon );

	/* Redraw characters */
	fbcon_redraw_characters ( fbcon );
}

/**
 * Clear portion of screen
 *
 * @v fbcon		Frame buffer console
 * @v ypos		Starting Y position
 */
static void fbcon_clear ( struct fbcon *fbcon, unsigned int ypos ) {
	struct fbcon_text_cell cell = {
		.foreground = fbcon->foreground,
		.background = fbcon->background,
		.character = ' ',
	};
	size_t offset;
	unsigned int xpos;

	/* Clear stored character array */
	for ( ; ypos < fbcon->character.height ; ypos++ ) {
		offset = ( ypos * fbcon->character.width * sizeof ( cell ) );
		for ( xpos = 0 ; xpos < fbcon->character.width ; xpos++ ) {
			copy_to_user ( fbcon->text.start, offset, &cell,
				       sizeof ( cell ) );
			offset += sizeof ( cell );
		}
	}

	/* Redraw screen */
	fbcon_redraw ( fbcon );
}

/**
 * Scroll screen
 *
 * @v fbcon		Frame buffer console
 */
static void fbcon_scroll ( struct fbcon *fbcon ) {
	size_t row_len;

	/* Sanity check */
	assert ( fbcon->ypos == fbcon->character.height );

	/* Scroll up character array */
	row_len = ( fbcon->character.width * sizeof ( struct fbcon_text_cell ));
	memmove_user ( fbcon->text.start, 0, fbcon->text.start, row_len,
		       ( row_len * ( fbcon->character.height - 1 ) ) );
	fbcon_clear ( fbcon, ( fbcon->character.height - 1 ) );

	/* Update cursor position */
	fbcon->ypos--;
}

/**
 * Draw character at cursor position
 *
 * @v fbcon		Frame buffer console
 * @v show_cursor	Show cursor
 */
static void fbcon_draw_cursor ( struct fbcon *fbcon, int show_cursor ) {
	struct fbcon_text_cell cell;
	size_t offset;

	offset = ( ( ( fbcon->ypos * fbcon->character.width ) + fbcon->xpos ) *
		   sizeof ( cell ) );
	copy_from_user ( &cell, fbcon->text.start, offset, sizeof ( cell ) );
	if ( show_cursor ) {
		cell.background = fbcon->foreground;
		cell.foreground = ( ( fbcon->background == FBCON_TRANSPARENT ) ?
				    0 : fbcon->background );
	}
	fbcon_draw_character ( fbcon, &cell, fbcon->xpos, fbcon->ypos );
}

/**
 * Handle ANSI CUP (cursor position)
 *
 * @v ctx		ANSI escape sequence context
 * @v count		Parameter count
 * @v params[0]		Row (1 is top)
 * @v params[1]		Column (1 is left)
 */
static void fbcon_handle_cup ( struct ansiesc_context *ctx,
			       unsigned int count __unused, int params[] ) {
	struct fbcon *fbcon = container_of ( ctx, struct fbcon, ctx );
	int cx = ( params[1] - 1 );
	int cy = ( params[0] - 1 );

	fbcon_draw_cursor ( fbcon, 0 );
	fbcon->xpos = cx;
	if ( fbcon->xpos >= fbcon->character.width )
		fbcon->xpos = ( fbcon->character.width - 1 );
	fbcon->ypos = cy;
	if ( fbcon->ypos >= fbcon->character.height )
		fbcon->ypos = ( fbcon->character.height - 1 );
	fbcon_draw_cursor ( fbcon, fbcon->show_cursor );
}

/**
 * Handle ANSI ED (erase in page)
 *
 * @v ctx		ANSI escape sequence context
 * @v count		Parameter count
 * @v params[0]		Region to erase
 */
static void fbcon_handle_ed ( struct ansiesc_context *ctx,
			      unsigned int count __unused,
			      int params[] __unused ) {
	struct fbcon *fbcon = container_of ( ctx, struct fbcon, ctx );

	/* We assume that we always clear the whole screen */
	assert ( params[0] == ANSIESC_ED_ALL );

	/* Clear screen */
	fbcon_clear ( fbcon, 0 );

	/* Reset cursor position */
	fbcon->xpos = 0;
	fbcon->ypos = 0;
	fbcon_draw_cursor ( fbcon, fbcon->show_cursor );
}

/**
 * Handle ANSI SGR (set graphics rendition)
 *
 * @v ctx		ANSI escape sequence context
 * @v count		Parameter count
 * @v params		List of graphic rendition aspects
 */
static void fbcon_handle_sgr ( struct ansiesc_context *ctx, unsigned int count,
			       int params[] ) {
	struct fbcon *fbcon = container_of ( ctx, struct fbcon, ctx );
	uint32_t *custom = NULL;
	uint32_t rgb;
	unsigned int end;
	unsigned int i;
	int aspect;

	for ( i = 0 ; i < count ; i++ ) {

		/* Process aspect */
		aspect = params[i];
		if ( aspect == 0 ) {
			fbcon_set_default_foreground ( fbcon );
			fbcon_set_default_background ( fbcon );
		} else if ( aspect == 1 ) {
			fbcon->bold = fbcon_colour ( fbcon, FBCON_BOLD );
		} else if ( aspect == 22 ) {
			fbcon->bold = 0;
		} else if ( ( aspect >= 30 ) && ( aspect <= 37 ) ) {
			fbcon->foreground =
				fbcon_ansi_colour ( fbcon, aspect - 30 );
		} else if ( aspect == 38 ) {
			custom = &fbcon->foreground;
		} else if ( aspect == 39 ) {
			fbcon_set_default_foreground ( fbcon );
		} else if ( ( aspect >= 40 ) && ( aspect <= 47 ) ) {
			fbcon->background =
				fbcon_ansi_colour ( fbcon, aspect - 40 );
		} else if ( aspect == 48 ) {
			custom = &fbcon->background;
		} else if ( aspect == 49 ) {
			fbcon_set_default_background ( fbcon );
		}

		/* Process custom RGB colour, if applicable
		 *
		 * We support the xterm-compatible
		 * "<ESC>[38;2;<red>;<green>;<blue>m" and
		 * "<ESC>[48;2;<red>;<green>;<blue>m" sequences.
		 */
		if ( custom ) {
			rgb = 0;
			end = ( i + 5 );
			for ( ; ( i < count ) && ( i < end ) ; i++ )
				rgb = ( ( rgb << 8 ) | params[i] );
			*custom = fbcon_colour ( fbcon, rgb );
			custom = NULL;
		}
	}
}

/**
 * Handle ANSI DECTCEM set (show cursor)
 *
 * @v ctx		ANSI escape sequence context
 * @v count		Parameter count
 * @v params		List of graphic rendition aspects
 */
static void fbcon_handle_dectcem_set ( struct ansiesc_context *ctx,
				       unsigned int count __unused,
				       int params[] __unused ) {
	struct fbcon *fbcon = container_of ( ctx, struct fbcon, ctx );

	fbcon->show_cursor = 1;
	fbcon_draw_cursor ( fbcon, 1 );
}

/**
 * Handle ANSI DECTCEM reset (hide cursor)
 *
 * @v ctx		ANSI escape sequence context
 * @v count		Parameter count
 * @v params		List of graphic rendition aspects
 */
static void fbcon_handle_dectcem_reset ( struct ansiesc_context *ctx,
					 unsigned int count __unused,
					 int params[] __unused ) {
	struct fbcon *fbcon = container_of ( ctx, struct fbcon, ctx );

	fbcon->show_cursor = 0;
	fbcon_draw_cursor ( fbcon, 0 );
}

/** ANSI escape sequence handlers */
static struct ansiesc_handler fbcon_ansiesc_handlers[] = {
	{ ANSIESC_CUP, fbcon_handle_cup },
	{ ANSIESC_ED, fbcon_handle_ed },
	{ ANSIESC_SGR, fbcon_handle_sgr },
	{ ANSIESC_DECTCEM_SET, fbcon_handle_dectcem_set },
	{ ANSIESC_DECTCEM_RESET, fbcon_handle_dectcem_reset },
	{ 0, NULL }
};

/**
 * Print a character to current cursor position
 *
 * @v fbcon		Frame buffer console
 * @v character		Character
 */
void fbcon_putchar ( struct fbcon *fbcon, int character ) {
	struct fbcon_text_cell cell;

	/* Intercept ANSI escape sequences */
	character = ansiesc_process ( &fbcon->ctx, character );
	if ( character < 0 )
		return;

	/* Handle control characters */
	switch ( character ) {
	case '\r':
		fbcon_draw_cursor ( fbcon, 0 );
		fbcon->xpos = 0;
		break;
	case '\n':
		fbcon_draw_cursor ( fbcon, 0 );
		fbcon->xpos = 0;
		fbcon->ypos++;
		break;
	case '\b':
		fbcon_draw_cursor ( fbcon, 0 );
		if ( fbcon->xpos ) {
			fbcon->xpos--;
		} else if ( fbcon->ypos ) {
			fbcon->xpos = ( fbcon->character.width - 1 );
			fbcon->ypos--;
		}
		break;
	default:
		/* Print character at current cursor position */
		cell.foreground = ( fbcon->foreground | fbcon->bold );
		cell.background = fbcon->background;
		cell.character = character;
		fbcon_store_character ( fbcon, &cell, fbcon->xpos, fbcon->ypos);
		fbcon_draw_character ( fbcon, &cell, fbcon->xpos, fbcon->ypos );

		/* Advance cursor */
		fbcon->xpos++;
		if ( fbcon->xpos >= fbcon->character.width ) {
			fbcon->xpos = 0;
			fbcon->ypos++;
		}
		break;
	}

	/* Scroll screen if necessary */
	if ( fbcon->ypos >= fbcon->character.height )
		fbcon_scroll ( fbcon );

	/* Show cursor */
	fbcon_draw_cursor ( fbcon, fbcon->show_cursor );
}

/**
 * Calculate character geometry from pixel geometry
 *
 * @v pixel		Pixel geometry
 * @v character		Character geometry to fill in
 */
static void fbcon_char_geometry ( const struct fbcon_geometry *pixel,
				  struct fbcon_geometry *character ) {

	character->width = ( pixel->width / FBCON_CHAR_WIDTH );
	character->height = ( pixel->height / FBCON_CHAR_HEIGHT );
	character->len = ( pixel->len * FBCON_CHAR_WIDTH );
	character->stride = ( pixel->stride * FBCON_CHAR_HEIGHT );
}

/**
 * Calculate margins from pixel geometry
 *
 * @v pixel		Pixel geometry
 * @v margin		Margins to fill in
 */
static void fbcon_margin ( const struct fbcon_geometry *pixel,
			   struct fbcon_margin *margin ) {
	unsigned int xgap;
	unsigned int ygap;

	xgap = ( pixel->width % FBCON_CHAR_WIDTH );
	ygap = ( pixel->height % FBCON_CHAR_HEIGHT );
	margin->left = ( xgap / 2 );
	margin->top = ( ygap / 2 );
	margin->right = ( pixel->width - ( xgap - margin->left ) );
	margin->bottom = ( pixel->height - ( ygap - margin->top ) );
}

/**
 * Align to first indented boundary
 *
 * @v value		Original value
 * @v blocksize		Block size
 * @v indent		Indent
 * @v max		Maximum allowed value
 * @ret value		Aligned value
 */
static unsigned int fbcon_align ( unsigned int value, unsigned int blocksize,
				  unsigned int indent, unsigned int max ) {
	unsigned int overhang;

	/* Special case: 0 is always a boundary regardless of the indent */
	if ( value == 0 )
		return value;

	/* Special case: first boundary is the indent */
	if ( value < indent )
		return indent;

	/* Round up to next indented boundary */
	overhang = ( ( value - indent ) % blocksize );
	value = ( value + ( ( blocksize - overhang ) % blocksize ) );

	/* Limit to maximum value */
	if ( value > max )
		value = max;

	return value;
}

/**
 * Initialise background picture
 *
 * @v fbcon		Frame buffer console
 * @v pixbuf		Background picture
 * @ret rc		Return status code
 */
static int fbcon_picture_init ( struct fbcon *fbcon,
				struct pixel_buffer *pixbuf ) {
	struct fbcon_picture *picture = &fbcon->picture;
	size_t pixel_len = fbcon->pixel->len;
	size_t len;
	size_t offset;
	size_t pixbuf_offset;
	uint32_t rgb;
	uint32_t raw;
	unsigned int x;
	unsigned int y;
	int rc;

	/* Calculate pixel geometry */
	picture->pixel.width = fbcon_align ( pixbuf->width, FBCON_CHAR_WIDTH,
					     fbcon->margin.left,
					     fbcon->pixel->width );
	picture->pixel.height = fbcon_align ( pixbuf->height, FBCON_CHAR_HEIGHT,
					      fbcon->margin.top,
					      fbcon->pixel->height );
	picture->pixel.len = pixel_len;
	picture->pixel.stride = ( picture->pixel.width * picture->pixel.len );

	/* Calculate character geometry */
	fbcon_char_geometry ( &picture->pixel, &picture->character );

	/* Calculate margins */
	memcpy ( &picture->margin, &fbcon->margin, sizeof ( picture->margin ) );
	if ( picture->margin.left > picture->pixel.width )
		picture->margin.left = picture->pixel.width;
	if ( picture->margin.top > picture->pixel.height )
		picture->margin.top = picture->pixel.height;
	if ( picture->margin.right > picture->pixel.width )
		picture->margin.right = picture->pixel.width;
	if ( picture->margin.bottom > picture->pixel.height )
		picture->margin.bottom = picture->pixel.height;
	picture->indent = ( ( picture->margin.top * picture->pixel.stride ) +
			    ( picture->margin.left * picture->pixel.len ) );
	DBGC ( fbcon, "FBCON %p picture is pixel %dx%d, char %dx%d at "
	       "[%d-%d),[%d-%d)\n", fbcon, picture->pixel.width,
	       picture->pixel.height, picture->character.width,
	       picture->character.height, picture->margin.left,
	       picture->margin.right, picture->margin.top,
	       picture->margin.bottom );

	/* Allocate buffer */
	len = ( picture->pixel.width * picture->pixel.height *
		picture->pixel.len );
	picture->start = umalloc ( len );
	if ( ! picture->start ) {
		DBGC ( fbcon, "FBCON %p could not allocate %zd bytes for "
		       "picture\n", fbcon, len );
		rc = -ENOMEM;
		goto err_umalloc;
	}

	/* Convert to frame buffer raw format */
	memset_user ( picture->start, 0, 0, len );
	pixbuf_offset = 0;
	for ( y = 0 ; ( y < pixbuf->height ) &&
		      ( y < picture->pixel.height ) ; y++ ) {
		offset = ( y * picture->pixel.stride );
		pixbuf_offset = ( y * pixbuf->width * sizeof ( rgb ) );
		for ( x = 0 ; ( x < pixbuf->width ) &&
			      ( x < picture->pixel.width ) ; x++ ) {
			copy_from_user ( &rgb, pixbuf->data, pixbuf_offset,
					 sizeof ( rgb ) );
			raw = fbcon_colour ( fbcon, rgb );
			copy_to_user ( picture->start, offset, &raw, pixel_len);
			offset += pixel_len;
			pixbuf_offset += sizeof ( rgb );
		}
	}

	return 0;

	ufree ( picture->start );
 err_umalloc:
	return rc;
}

/**
 * Initialise frame buffer console
 *
 * @v fbcon		Frame buffer console
 * @v start		Start address
 * @v pixel		Pixel geometry
 * @v map		Colour mapping
 * @v font		Font definition
 * @v pixbuf		Background picture (if any)
 * @ret rc		Return status code
 */
int fbcon_init ( struct fbcon *fbcon, userptr_t start,
		 struct fbcon_geometry *pixel,
		 struct fbcon_colour_map *map,
		 struct fbcon_font *font,
		 struct pixel_buffer *pixbuf ) {
	int rc;

	/* Initialise data structure */
	memset ( fbcon, 0, sizeof ( *fbcon ) );
	fbcon->start = start;
	fbcon->pixel = pixel;
	assert ( pixel->len <= sizeof ( uint32_t ) );
	fbcon->map = map;
	fbcon->font = font;
	fbcon->ctx.handlers = fbcon_ansiesc_handlers;
	fbcon->show_cursor = 1;

	/* Derive overall length */
	fbcon->len = ( pixel->height * pixel->stride );
	DBGC ( fbcon, "FBCON %p at [%08lx,%08lx)\n", fbcon,
	       user_to_phys ( fbcon->start, 0 ),
	       user_to_phys ( fbcon->start, fbcon->len ) );

	/* Derive character geometry from pixel geometry */
	fbcon_char_geometry ( pixel, &fbcon->character );
	fbcon_margin ( pixel, &fbcon->margin );
	fbcon->indent = ( ( fbcon->margin.top * pixel->stride ) +
			  ( fbcon->margin.left * pixel->len ) );
	DBGC ( fbcon, "FBCON %p is pixel %dx%d, char %dx%d at "
	       "[%d-%d),[%d-%d)\n", fbcon, fbcon->pixel->width,
	       fbcon->pixel->height, fbcon->character.width,
	       fbcon->character.height, fbcon->margin.left, fbcon->margin.right,
	       fbcon->margin.top, fbcon->margin.bottom );

	/* Set default colours */
	fbcon_set_default_foreground ( fbcon );
	fbcon_set_default_background ( fbcon );

	/* Allocate stored character array */
	fbcon->text.start = umalloc ( fbcon->character.width *
				      fbcon->character.height *
				      sizeof ( struct fbcon_text_cell ) );
	if ( ! fbcon->text.start ) {
		rc = -ENOMEM;
		goto err_text;
	}

	/* Generate pixel buffer from background image, if applicable */
	if ( pixbuf && ( ( rc = fbcon_picture_init ( fbcon, pixbuf ) ) != 0 ) )
		goto err_picture;

	/* Clear screen */
	fbcon_clear ( fbcon, 0 );

	/* Update console width and height */
	console_set_size ( fbcon->character.width, fbcon->character.height );

	return 0;

	ufree ( fbcon->picture.start );
 err_picture:
	ufree ( fbcon->text.start );
 err_text:
	return rc;
}

/**
 * Finalise frame buffer console
 *
 * @v fbcon		Frame buffer console
 */
void fbcon_fini ( struct fbcon *fbcon ) {

	ufree ( fbcon->text.start );
	ufree ( fbcon->picture.start );
}
