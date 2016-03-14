#ifndef _EDID_H
#define _EDID_H

/** @file
 *
 * EDID
 *
 */

FILE_LICENCE ( MIT );

typedef unsigned char uchar;

typedef enum {
	UNDEFINED,
	DVI,
	HDMI_A,
	HDMI_B,
	MDDI,
	DISPLAY_PORT
} Interface;

typedef enum {
	UNDEFINED_COLOR,
	MONOCHROME,
	RGB,
	OTHER_COLOR
} ColorType;

typedef enum {
	NO_STEREO,
	FIELD_RIGHT,
	FIELD_LEFT,
	TWO_WAY_RIGHT_ON_EVEN,
	TWO_WAY_LEFT_ON_EVEN,
	FOUR_WAY_INTERLEAVED,
	SIDE_BY_SIDE
} StereoType;

typedef struct Timing {
	int width;
	int height;
	int frequency;
} Timing;

struct display_descriptor {
};

typedef struct detailed_timing {
	int		pixel_clock;
	int		h_addr;
	int		h_blank;
	int		h_sync;
	int		h_front_porch;
	int		v_addr;
	int		v_blank;
	int		v_sync;
	int		v_front_porch;
	int		width_mm;
	int		height_mm;
	int		right_border;
	int		top_border;
	int		interlaced;
	StereoType	stereo;

	int		digital_sync;
	union {
		struct {
			int bipolar;
			int serrations;
			int sync_on_green;
		} analog;

		struct {
			int composite;
			int serrations;
			int negative_vsync;
			int negative_hsync;
		} digital;
	};
} detailed_timing;

typedef struct edid_info {
	int		checksum;
	char		manufacturer_code[4];
	int		product_code;
	unsigned int	serial_number;

	int		production_week;	/* -1 if not specified */
	int		production_year;	/* -1 if not specified */
	int		model_year;		/* -1 if not specified */

	int		major_version;
	int		minor_version;

	int		is_digital;

	union {
		struct {
			int		bits_per_primary;
			Interface	interface;
			int		rgb444;
			int		ycrcb444;
			int		ycrcb422;
		} digital;

		struct {
			double		video_signal_level;
			double		sync_signal_level;
			double		total_signal_level;

			int		blank_to_black;

			int		separate_hv_sync;
			int		composite_sync_on_h;
			int		composite_sync_on_green;
			int		serration_on_vsync;
			ColorType	color_type;
		} analog;
	};

	int		width_mm;		/* -1 if not specified */
	int		height_mm;		/* -1 if not specified */
	double		aspect_ratio;		/* -1.0 if not specififed */

	double		gamma;			/* -1.0 if not specified */

	int		standby;
	int		suspend;
	int		active_off;

	int		srgb_is_standard;
	int		preferred_timing_includes_native;
	int		continuous_frequency;

	double		red_x;
	double		red_y;
	double		green_x;
	double		green_y;
	double		blue_x;
	double		blue_y;
	double		white_x;
	double		white_y;

	Timing		established[24];	/* Terminated by 0x0x0 */
	Timing		standard[8];

	int		n_detailed_timings;
	detailed_timing	detailed_timings[4];	/* If monitor has a preferred
						 * mode, it is the first one
						 * (whether it has, is
						 * determined by the
						 * preferred_timing_includes
						 * bit.
						 */

	/* Optional product description */
	char		dsc_serial_number[14];
	char		dsc_product_name[14];
	char		dsc_string[14];		/* Unspecified ASCII data */
} edid_info;

/**
 * Decode EDID
 *
 * @v   edid		String containing EDID
 * @v   info		Struct containing decoded EDID information
 * @ret rc		Return status code
 */
extern int edid_decode ( const uchar *edid, edid_info *info );

/**
 * Get preferred resolution from EDID
 *
 * @v   info		EDID information
 * @v   x		X resolution of preferred resolution
 * @v   y		Y resolution of preferred resolution
 * @ret rc		Return status code
 */
extern int edid_get_preferred_resolution ( edid_info *info,
					   unsigned int *x, unsigned int *y );

/**
 * Dump EDID (debug level must be set to 2 for this to do anything)
 *
 * @v   info		EDID information
 */
extern void edid_dump_monitor_info (edid_info *info);

#endif /* _EDID_H */
