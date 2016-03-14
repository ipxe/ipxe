/*
 * Copyright 2007 Red Hat, Inc.
 * Copyright 2016 Jonathan Dieter <jdieter@lesbg.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Author: Soren Sandmann <sandmann@redhat.com> */

FILE_LICENCE ( MIT );

#include <ipxe/umalloc.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <edid.h>

/**
 * Get a single bit from an int
 *
 * @v   in		Int to extract bit from
 * @v   bit		Position of bit to extract
 * @ret value		Value of bit
 */
static int get_bit ( int in, int bit ) {
	return ( in & ( 1 << bit ) ) >> bit;
}

/**
 * Get a range of bits from an int
 *
 * @v   in		Int to extract bits from
 * @v   begin		Position of first bit to extract
 * @v   end		Position of last bit to extract
 * @ret value		Value of bits
 */
static int get_bits ( int in, int begin, int end ) {
	int mask = ( 1 << ( end - begin + 1 ) ) - 1;

	return ( in >> begin ) & mask;
}

/**
 * Check whether header is valid
 *
 * @v   edid		String containing EDID
 * @ret rc		False if header isn't valid
 */
static int decode_header ( const uchar *edid ) {
	if ( memcmp ( edid, "\x00\xff\xff\xff\xff\xff\xff\x00", 8 ) == 0 )
		return TRUE;
	return FALSE;
}

/**
 * Decode vendor and product information
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put information into
 * @ret rc		Return status code
 */
static int decode_vendor_and_product_identification ( const uchar *edid,
						      edid_info *info ) {
	int is_model_year;

	/* Manufacturer Code */
	info->manufacturer_code[0]  = get_bits ( edid[0x08], 2, 6 );
	info->manufacturer_code[1]  = get_bits ( edid[0x08], 0, 1 ) << 3;
	info->manufacturer_code[1] |= get_bits ( edid[0x09], 5, 7 );
	info->manufacturer_code[2]  = get_bits ( edid[0x09], 0, 4 );
	info->manufacturer_code[3]  = '\0';

	info->manufacturer_code[0] += 'A' - 1;
	info->manufacturer_code[1] += 'A' - 1;
	info->manufacturer_code[2] += 'A' - 1;

	/* Product Code */
	info->product_code = edid[0x0b] << 8 | edid[0x0a];

	/* Serial Number */
	info->serial_number = edid[0x0c] | edid[0x0d] << 8 |
			      edid[0x0e] << 16 | edid[0x0f] << 24;

	/* Week and Year */
	is_model_year = FALSE;
	switch ( edid[0x10] ) {
	case 0x00:
		info->production_week = -1;
		break;

	case 0xff:
		info->production_week = -1;
		is_model_year = TRUE;
		break;

	default:
		info->production_week = edid[0x10];
		break;
	}

	if ( is_model_year ) {
		info->production_year = -1;
		info->model_year = 1990 + edid[0x11];
	} else {
		info->production_year = 1990 + edid[0x11];
		info->model_year = -1;
	}

	return TRUE;
}

/**
 * Decode EDID version
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put version into
 * @ret rc		Return status code
 */
static int decode_edid_version (const uchar *edid, edid_info *info) {
	info->major_version = edid[0x12];
	info->minor_version = edid[0x13];

	return TRUE;
}

/**
 * Decode display parameters
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put display parameters into
 * @ret rc		Return status code
 */
static int decode_display_parameters ( const uchar *edid, edid_info *info ) {
	/* Digital vs Analog */
	info->is_digital = get_bit ( edid[0x14], 7 );

	if ( info->is_digital )	{
		int bits;

		static const int bit_depth[8] =
		{
			-1, 6, 8, 10, 12, 14, 16, -1
		};

		static const Interface interfaces[6] =
		{
			UNDEFINED, DVI, HDMI_A, HDMI_B, MDDI, DISPLAY_PORT
		};

		bits = get_bits ( edid[0x14], 4, 6 );
		info->digital.bits_per_primary = bit_depth[bits];

		bits = get_bits ( edid[0x14], 0, 3 );

		if ( bits <= 5 )
			info->digital.interface = interfaces[bits];
		else
			info->digital.interface = UNDEFINED;
	} else {
		int bits = get_bits (edid[0x14], 5, 6);

		static const double levels[][3] =
		{
			{ 0.7,   0.3,	1.0 },
			{ 0.714, 0.286,  1.0 },
			{ 1.0,   0.4,	1.4 },
			{ 0.7,   0.0,	0.7 },
		};

		info->analog.video_signal_level = levels[bits][0];
		info->analog.sync_signal_level = levels[bits][1];
		info->analog.total_signal_level = levels[bits][2];

		info->analog.blank_to_black = get_bit ( edid[0x14], 4 );

		info->analog.separate_hv_sync = get_bit ( edid[0x14], 3 );
		info->analog.composite_sync_on_h = get_bit ( edid[0x14], 2 );
		info->analog.composite_sync_on_green = get_bit ( edid[0x14],
								 1 );

		info->analog.serration_on_vsync = get_bit ( edid[0x14], 0 );
	}

	/* Screen Size / Aspect Ratio */
	if ( ( edid[0x15] == 0 ) && ( edid[0x16] == 0 ) ) {
		info->width_mm = -1;
		info->height_mm = -1;
		info->aspect_ratio = -1.0;
	} else if ( edid[0x16] == 0 ) {
		info->width_mm = -1;
		info->height_mm = -1;
		info->aspect_ratio = 100.0 / (edid[0x15] + 99);
	} else if ( edid[0x15] == 0 ) {
		info->width_mm = -1;
		info->height_mm = -1;
		info->aspect_ratio = 100.0 / (edid[0x16] + 99);
		info->aspect_ratio = 1/info->aspect_ratio; /* portrait */
	} else {
		info->width_mm = 10 * edid[0x15];
		info->height_mm = 10 * edid[0x16];
	}

	/* Gamma */
	if ( edid[0x17] == 0xFF )
		info->gamma = -1.0;
	else
		info->gamma = ( edid[0x17] + 100.0 ) / 100.0;

	/* Features */
	info->standby = get_bit ( edid[0x18], 7 );
	info->suspend = get_bit ( edid[0x18], 6 );
	info->active_off = get_bit ( edid[0x18], 5 );

	if ( info->is_digital ) {
		info->digital.rgb444 = TRUE;
		if (get_bit (edid[0x18], 3))
			info->digital.ycrcb444 = 1;
		if (get_bit (edid[0x18], 4))
			info->digital.ycrcb422 = 1;
	} else {
		int bits = get_bits (edid[0x18], 3, 4);
		ColorType color_type[4] =
		{
			MONOCHROME, RGB, OTHER_COLOR, UNDEFINED_COLOR
		};

		info->analog.color_type = color_type[bits];
	}

	info->srgb_is_standard = get_bit ( edid[0x18], 2 );

	/* In 1.3 this is called "has preferred timing" */
	info->preferred_timing_includes_native = get_bit ( edid[0x18], 1 );

	/* FIXME: In 1.3 this indicates whether the monitor accepts GTF */
	info->continuous_frequency = get_bit ( edid[0x18], 0 );
	return TRUE;
}

/**
 * Convert fraction to double
 *
 * @v   high		Numerator
 * @v   low		Denominator
 * @ret value		Decimal equivalent of fraction
 */
static double decode_fraction (int high, int low) {
	return ( ( double ) high / ( double ) low );
}

/**
 * Decode color characteristics
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put color characteristics into
 * @ret rc		Return status code
 */
static int decode_color_characteristics ( const uchar *edid,
					  edid_info *info ) {
	info->red_x = decode_fraction ( edid[0x1b],
					get_bits ( edid[0x19], 6, 7 ) );
	info->red_y = decode_fraction ( edid[0x1c],
					get_bits ( edid[0x19], 5, 4 ) );
	info->green_x = decode_fraction ( edid[0x1d],
					  get_bits ( edid[0x19], 2, 3 ) );
	info->green_y = decode_fraction ( edid[0x1e],
					  get_bits ( edid[0x19], 0, 1 ) );
	info->blue_x = decode_fraction ( edid[0x1f],
					 get_bits ( edid[0x1a], 6, 7 ) );
	info->blue_y = decode_fraction ( edid[0x20],
					 get_bits ( edid[0x1a], 4, 5 ) );
	info->white_x = decode_fraction ( edid[0x21],
					  get_bits ( edid[0x1a], 2, 3 ) );
	info->white_y = decode_fraction ( edid[0x22],
					  get_bits ( edid[0x1a], 0, 1 ) );

	return TRUE;
}

/**
 * Decode established timings
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put timings into
 * @ret rc	      Return status code
 */
static int decode_established_timings ( const uchar *edid,
					edid_info *info ) {
	static const Timing established[][8] =
	{
		{
			{ 800, 600, 60 },
			{ 800, 600, 56 },
			{ 640, 480, 75 },
			{ 640, 480, 72 },
			{ 640, 480, 67 },
			{ 640, 480, 60 },
			{ 720, 400, 88 },
			{ 720, 400, 70 }
		},
		{
			{ 1280, 1024, 75 },
			{ 1024, 768, 75 },
			{ 1024, 768, 70 },
			{ 1024, 768, 60 },
			{ 1024, 768, 87 },
			{ 832, 624, 75 },
			{ 800, 600, 75 },
			{ 800, 600, 72 }
		},
		{
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 0, 0, 0 },
			{ 1152, 870, 75 }
		},
	};

	int i, j, idx;

	idx = 0;
	for ( i = 0; i < 3; ++i ) {
		for ( j = 0; j < 8; ++j ) {
			int byte = edid[0x23 + i];

			if ( get_bit ( byte, j ) &&
			     established[i][j].frequency != 0 )
				info->established[idx++] = established[i][j];
		}
	}
	return TRUE;
}

/**
 * Decode standard timings
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put timings into
 * @ret rc		Return status code
 */
static int decode_standard_timings ( const uchar *edid, edid_info *info ) {
	int i;

	for ( i = 0; i < 8; i++ ) {
		int first = edid[0x26 + 2 * i];
		int second = edid[0x27 + 2 * i];

		if ( first != 0x01 && second != 0x01 ) {
			int w = 8 * (first + 31);
			int h;

			switch ( get_bits ( second, 6, 7 ) ) {
			case 0x00: h = ( w / 16 ) * 10; break;
			case 0x01: h = ( w / 4 ) * 3; break;
			case 0x02: h = ( w / 5 ) * 4; break;
			case 0x03: h = ( w / 16 ) * 9; break;
			}

			info->standard[i].width = w;
			info->standard[i].height = h;
			info->standard[i].frequency =
				get_bits ( second, 0, 5 ) + 60;
		}
	}

	return TRUE;
}

/**
 * Decode lf string into normal string
 *
 * @v   s		lf string to decode
 * @v   n_chars		Number of characters to decode
 * @v   result		Decoded string
 */
static void decode_lf_string ( const uchar *s, int n_chars, char *result ) {
	int i;
	for ( i = 0; i < n_chars; ++i ) {
		if ( s[i] == 0x0a ) {
			*result++ = '\0';
			break;
		} else if ( s[i] == 0x00 ) {
			/* Convert embedded 0's to spaces */
			*result++ = ' ';
		} else {
			*result++ = s[i];
		}
	}
}

/**
 * Decode individual display descriptors
 *
 * @v   desc		String containing display descriptor
 * @v   info		Struct to put descriptor into
 */
static void decode_display_descriptor ( const uchar *desc,
					edid_info *info ) {
	switch ( desc[0x03] ) {
	case 0xFC:
		decode_lf_string ( desc + 5, 13, info->dsc_product_name );
		break;
	case 0xFF:
		decode_lf_string ( desc + 5, 13, info->dsc_serial_number );
		break;
	case 0xFE:
		decode_lf_string ( desc + 5, 13, info->dsc_string );
		break;
	case 0xFD:
		/* Range Limits */
		break;
	case 0xFB:
		/* Color Point */
		break;
	case 0xFA:
		/* Timing Identifications */
		break;
	case 0xF9:
		/* Color Management */
		break;
	case 0xF8:
		/* Timing Codes */
		break;
	case 0xF7:
		/* Established Timings */
		break;
	case 0x10:
		break;
	}
}

/**
 * Decode detailed timings
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put timings into
 * @ret rc		Return status code
 */
static void decode_detailed_timing ( const uchar *timing,
				     detailed_timing *detailed ) {
	int bits;
	StereoType stereo[] =
	{
		NO_STEREO, NO_STEREO, FIELD_RIGHT, FIELD_LEFT,
		TWO_WAY_RIGHT_ON_EVEN, TWO_WAY_LEFT_ON_EVEN,
		FOUR_WAY_INTERLEAVED, SIDE_BY_SIDE
	};

	detailed->pixel_clock = ( timing[0x00] | timing[0x01] << 8 ) * 10000;
	detailed->h_addr = timing[0x02] | ( ( timing[0x04] & 0xf0 ) << 4 );
	detailed->h_blank = timing[0x03] | ( ( timing[0x04] & 0x0f ) << 8 );
	detailed->v_addr = timing[0x05] | ( ( timing[0x07] & 0xf0 ) << 4 );
	detailed->v_blank = timing[0x06] | ( ( timing[0x07] & 0x0f ) << 8 );
	detailed->h_front_porch = timing[0x08] |
				  get_bits ( timing[0x0b], 6, 7 ) << 8;
	detailed->h_sync = timing[0x09] | get_bits ( timing[0x0b], 4, 5 ) << 8;
	detailed->v_front_porch = get_bits ( timing[0x0a], 4, 7 ) |
				  get_bits ( timing[0x0b], 2, 3 ) << 4;
	detailed->v_sync = get_bits ( timing[0x0a], 0, 3 ) |
			   get_bits ( timing[0x0b], 0, 1 ) << 4;
	detailed->width_mm =  timing[0x0c] |
				  get_bits ( timing[0x0e], 4, 7 ) << 8;
	detailed->height_mm = timing[0x0d] |
				  get_bits ( timing[0x0e], 0, 3 ) << 8;
	detailed->right_border = timing[0x0f];
	detailed->top_border = timing[0x10];

	detailed->interlaced = get_bit ( timing[0x11], 7 );

	/* Stereo */
	bits = get_bits ( timing[0x11], 5, 6 ) << 1 |
		   get_bit ( timing[0x11], 0 );
	detailed->stereo = stereo[bits];

	/* Sync */
	bits = timing[0x11];

	detailed->digital_sync = get_bit ( bits, 4 );
	if ( detailed->digital_sync ) {
		detailed->digital.composite = !get_bit (bits, 3);

		if (detailed->digital.composite) {
			detailed->digital.serrations = get_bit ( bits, 2 );
			detailed->digital.negative_vsync = FALSE;
		} else {
			detailed->digital.serrations = FALSE;
			detailed->digital.negative_vsync =
				! get_bit ( bits, 2 );
		}

		detailed->digital.negative_hsync = ! get_bit ( bits, 0 );
	} else {
		detailed->analog.bipolar = get_bit ( bits, 3 );
		detailed->analog.serrations = get_bit ( bits, 2 );
		detailed->analog.sync_on_green = ! get_bit ( bits, 1 );
	}
}

/**
 * Decode descriptors
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put descriptors into
 * @ret rc		Return status code
 */
static int decode_descriptors ( const uchar *edid, edid_info *info ) {
	int i;
	int timing_idx;

	timing_idx = 0;

	for (i = 0; i < 4; ++i) {
		int index = 0x36 + i * 18;

		if ( edid[index + 0] == 0x00 && edid[index + 1] == 0x00 ) {
			decode_display_descriptor ( edid + index, info );
		} else {
			decode_detailed_timing (
				edid + index,
				&( info->detailed_timings[timing_idx++] ) );
		}
	}

	info->n_detailed_timings = timing_idx;

	return TRUE;
}

/**
 * Decode checksum
 *
 * @v   edid		String containing EDID
 * @v   info		Struct to put checksum into
 * @ret rc		Fail if checksum is invalid
 */
static int decode_check_sum ( const uchar *edid, edid_info *info ) {
	int i;
	uchar check = 0;

	for ( i = 0; i < 128; ++i )
		check += edid[i];

	info->checksum = check;

	/* Checksum should be 0 */
	if ( ! check )
		return TRUE;
	else
		return FALSE;
}

/**
 * Give string yes/no equivalent of boolean
 *
 * @v   v		Boolean input
 * @ret s		"yes" or "no"
 */
static const char *yesno ( int v ) {
	return v? "yes" : "no";
}

/**
 * Decode EDID
 *
 * @v   edid		String containing EDID
 * @v   info		Struct containing decoded EDID information
 * @ret rc		Return status code
 */
int edid_decode ( const uchar *edid, edid_info *info ) {
	/* Fail hard if checksum isn't valid.  I'm not sure if this is
	 * actually necessary.  It might make more sense to just keep going */
	if ( ! decode_check_sum ( edid, info ) ) {
		int rc = -EILSEQ;
		DBGC ( &info, "EDID checksum %d invalid - should be 0: %s\n",
		       info->checksum, strerror ( rc ) );
		return FALSE;
	}

	if ( ! decode_header ( edid ) ) {
		DBGC ( &info, "EDID header decode failed\n" );
		return FALSE;
	}

	if ( ! decode_vendor_and_product_identification ( edid, info ) ) {
		DBGC ( &info, "EDID vendor and product id decode failed\n" );
		return FALSE;
	}

	if ( ! decode_edid_version ( edid, info ) ) {
		DBGC ( &info, "EDID version decode failed\n" );
		return FALSE;
	}

	if ( ! decode_display_parameters ( edid, info ) ) {
		DBGC ( &info, "EDID display parameters decode failed\n" );
		return FALSE;
	}

	if ( ! decode_color_characteristics ( edid, info ) ) {
		DBGC ( &info, "EDID color characteristics decode failed\n" );
		return FALSE;
	}

	if ( ! decode_established_timings ( edid, info ) ) {
		DBGC ( &info, "EDID established timings decode failed\n" );
		return FALSE;
	}

	if ( ! decode_standard_timings ( edid, info ) ) {
		DBGC ( &info, "EDID standard timings decode failed\n" );
		return FALSE;
	}

	if ( ! decode_descriptors ( edid, info ) ) {
		DBGC ( &info, "EDID descriptors decode failed\n" );
		return FALSE;
	}

	return TRUE;
}

/**
 * Get preferred resolution from EDID
 *
 * @v   info		EDID information
 * @v   x		X resolution of preferred resolution
 * @v   y		Y resolution of preferred resolution
 * @ret rc		Return status code
 */
int edid_get_preferred_resolution ( edid_info *info,
				    unsigned int *x, unsigned int *y ) {
	if ( info->preferred_timing_includes_native ) {
		*x = info->detailed_timings[0].h_addr;
		*y = info->detailed_timings[0].v_addr;
		return TRUE;
	} else {
		return FALSE;
	}
}
/**
 * Dump EDID (debug level must be set to 2 for this to do anything)
 *
 * @v   info		EDID information
 */
void edid_dump_monitor_info ( edid_info *info ) {
	char *s = NULL;
	int   i;

	DBGC2 ( &info, "Checksum: %d (%s)\n",
		info->checksum, info->checksum? "incorrect" : "correct");
	DBGC2 ( &info, "Manufacturer Code: %s\n", info->manufacturer_code);
	DBGC2 ( &info, "Product Code: 0x%x\n", info->product_code);
	DBGC2 ( &info, "Serial Number: %u\n", info->serial_number);

	if ( info->production_week != -1 )
		DBGC2 ( &info, "Production Week: %d\n",
			info->production_week );
	else
		DBGC2 ( &info, "Production Week: unspecified\n");

	if ( info->production_year != -1 )
		DBGC2 ( &info, "Production Year: %d\n",
			info->production_year );
	else
		DBGC2 ( &info, "Production Year: unspecified\n" );

	if ( info->model_year != -1 )
		DBGC2 ( &info, "Model Year: %d\n", info->model_year );
	else
		DBGC2 ( &info, "Model Year: unspecified\n" );

	DBGC2 ( &info, "EDID revision: %d.%d\n", info->major_version,
		info->minor_version );

	DBGC2 ( &info, "Display is %s\n",
		info->is_digital ? "digital" : "analog" );
	if ( info->is_digital )	{
		const char *interface = NULL;

		if ( info->digital.bits_per_primary != -1 )
			DBGC2 ( &info, "Bits Per Primary: %d\n",
				info->digital.bits_per_primary );
		else
			DBGC2 ( &info, "Bits Per Primary: undefined\n" );

		switch ( info->digital.interface ) {
		case DVI: interface = "DVI"; break;
		case HDMI_A: interface = "HDMI-a"; break;
		case HDMI_B: interface = "HDMI-b"; break;
		case MDDI: interface = "MDDI"; break;
		case DISPLAY_PORT: interface = "DisplayPort"; break;
		case UNDEFINED: interface = "undefined"; break;
		}
		DBGC2 ( &info, "Interface: %s\n", interface );

		DBGC2 ( &info, "RGB 4:4:4: %s\n",
			yesno ( info->digital.rgb444 ) );
		DBGC2 ( &info, "YCrCb 4:4:4: %s\n",
			yesno ( info->digital.ycrcb444 ) );
		DBGC2 ( &info, "YCrCb 4:2:2: %s\n",
			yesno ( info->digital.ycrcb422 ) );
	} else {
		DBGC2 ( &info, "Video Signal Level: %f\n",
			info->analog.video_signal_level );
		DBGC2 ( &info, "Sync Signal Level: %f\n",
			info->analog.sync_signal_level );
		DBGC2 ( &info, "Total Signal Level: %f\n",
			info->analog.total_signal_level );

		DBGC2 ( &info, "Blank to Black: %s\n",
			yesno ( info->analog.blank_to_black ) );
		DBGC2 ( &info, "Separate HV Sync: %s\n",
			yesno ( info->analog.separate_hv_sync ) );
		DBGC2 ( &info, "Composite Sync on H: %s\n",
			yesno ( info->analog.composite_sync_on_h ) );
		DBGC2 ( &info, "Serration on VSync: %s\n",
			yesno ( info->analog.serration_on_vsync ) );

		switch ( info->analog.color_type ) {
		case UNDEFINED_COLOR: s = "undefined"; break;
		case MONOCHROME: s = "monochrome"; break;
		case RGB: s = "rgb"; break;
		case OTHER_COLOR: s = "other color"; break;
		};

		DBGC2 ( &info, "Color: %s\n", s );
	}

	if ( info->width_mm == -1 )
		DBGC2 ( &info, "Width: undefined\n" );
	else
		DBGC2 ( &info, "Width: %d mm\n", info->width_mm );

	if ( info->height_mm == -1 )
		DBGC2 ( &info, "Height: undefined\n" );
	else
		DBGC2 ( &info, "Height: %d mm\n", info->height_mm );

	if ( info->aspect_ratio > 0 )
		DBGC2 ( &info, "Aspect Ratio: %f\n", info->aspect_ratio );
	else
		DBGC2 ( &info, "Aspect Ratio: undefined\n" );

	if ( info->gamma >= 0 )
		DBGC2 ( &info, "Gamma: %f\n", info->gamma );
	else
		DBGC2 ( &info, "Gamma: undefined\n" );

	DBGC2 ( &info, "Standby: %s\n", yesno ( info->standby ) );
	DBGC2 ( &info, "Suspend: %s\n", yesno ( info->suspend ) );
	DBGC2 ( &info, "Active Off: %s\n", yesno ( info->active_off ) );

	DBGC2 ( &info, "SRGB is Standard: %s\n",
		yesno ( info->srgb_is_standard ) );
	DBGC2 ( &info, "Preferred Timing Includes Native: %s\n",
		yesno ( info->preferred_timing_includes_native ) );
	DBGC2 ( &info, "Continuous Frequency: %s\n",
		yesno ( info->continuous_frequency ) );

	DBGC2 ( &info, "Red X: %f\n", info->red_x );
	DBGC2 ( &info, "Red Y: %f\n", info->red_y );
	DBGC2 ( &info, "Green X: %f\n", info->green_x );
	DBGC2 ( &info, "Green Y: %f\n", info->green_y );
	DBGC2 ( &info, "Blue X: %f\n", info->blue_x );
	DBGC2 ( &info, "Blue Y: %f\n", info->blue_y );
	DBGC2 ( &info, "White X: %f\n", info->white_x );
	DBGC2 ( &info, "White Y: %f\n", info->white_y );

	DBGC2 ( &info, "Established Timings:\n" );

	for ( i = 0; i < 24; ++i ) {
		Timing *timing = &( info->established[i] );

		if ( timing->frequency == 0 )
			break;

		DBGC2 ( &info, "  %d x %d @ %d Hz\n",
			timing->width, timing->height, timing->frequency );

	}

	DBGC2 ( &info, "Standard Timings:\n" );
	for ( i = 0; i < 8; ++i ) {
		Timing *timing = &( info->standard[i] );

		if ( timing->frequency == 0 )
			break;

		DBGC2 ( &info, "  %d x %d @ %d Hz\n",
			timing->width, timing->height, timing->frequency );
	}

	for ( i = 0; i < info->n_detailed_timings; ++i ) {
		detailed_timing *timing = &( info->detailed_timings[i] );
		const char *s = NULL;

		DBGC2 ( &info, "Timing%s: \n",
			( i == 0 && info->preferred_timing_includes_native ) ?
				    " (Preferred)" : "" );
		DBGC2 ( &info, "  Pixel Clock: %d\n", timing->pixel_clock );
		DBGC2 ( &info, "  H Addressable: %d\n", timing->h_addr );
		DBGC2 ( &info, "  H Blank: %d\n", timing->h_blank );
		DBGC2 ( &info, "  H Front Porch: %d\n", timing->h_front_porch );
		DBGC2 ( &info, "  H Sync: %d\n", timing->h_sync );
		DBGC2 ( &info, "  V Addressable: %d\n", timing->v_addr );
		DBGC2 ( &info, "  V Blank: %d\n", timing->v_blank );
		DBGC2 ( &info, "  V Front Porch: %d\n", timing->v_front_porch );
		DBGC2 ( &info, "  V Sync: %d\n", timing->v_sync );
		DBGC2 ( &info, "  Width: %d mm\n", timing->width_mm );
		DBGC2 ( &info, "  Height: %d mm\n", timing->height_mm );
		DBGC2 ( &info, "  Right Border: %d\n", timing->right_border );
		DBGC2 ( &info, "  Top Border: %d\n", timing->top_border );
		switch ( timing->stereo ) {
		case NO_STEREO:   s = "No Stereo"; break;
		case FIELD_RIGHT: s = "Field Sequential, Right on Sync"; break;
		case FIELD_LEFT:  s = "Field Sequential, Left on Sync"; break;
		case TWO_WAY_RIGHT_ON_EVEN: s = "Two-way, Right on Even"; break;
		case TWO_WAY_LEFT_ON_EVEN:  s = "Two-way, Left on Even"; break;
		case FOUR_WAY_INTERLEAVED:  s = "Four-way Interleaved"; break;
		case SIDE_BY_SIDE:		  s = "Side-by-Side"; break;
		}
		DBGC2 ( &info, "  Stereo: %s\n", s );

		if ( timing->digital_sync ) {
			DBGC2 ( &info, "  Digital Sync:\n" );
			DBGC2 ( &info, "    composite: %s\n",
				yesno ( timing->digital.composite ) );
			DBGC2 ( &info, "    serrations: %s\n",
				yesno ( timing->digital.serrations ) );
			DBGC2 ( &info, "    negative vsync: %s\n",
				yesno ( timing->digital.negative_vsync ) );
			DBGC2 ( &info, "    negative hsync: %s\n",
				yesno ( timing->digital.negative_hsync ) );
		} else {
			DBGC2 ( &info, "  Analog Sync:\n" );
			DBGC2 ( &info, "    bipolar: %s\n",
				yesno ( timing->analog.bipolar ) );
			DBGC2 ( &info, "    serrations: %s\n",
				yesno ( timing->analog.serrations ) );
			DBGC2 ( &info, "    sync on green: %s\n",
				yesno ( timing->analog.sync_on_green ) );
		}
	}

	DBGC2 ( &info, "Detailed Product information:\n" );
	DBGC2 ( &info, "  Product Name: %s\n", info->dsc_product_name );
	DBGC2 ( &info, "  Serial Number: %s\n", info->dsc_serial_number );
	DBGC2 ( &info, "  Unspecified String: %s\n", info->dsc_string );
}
