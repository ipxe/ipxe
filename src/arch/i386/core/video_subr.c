/*
 *
 * modified from linuxbios code
 * by Cai Qiang <rimy2000@hotmail.com>
 *
 */

#ifdef CONSOLE_DIRECT_VGA

#include <etherboot.h>
#include <vga.h>

static char *vidmem;		/* The video buffer */
static int video_line, video_col;

#define VIDBUFFER 0xB8000	

static void memsetw(void *s, int c, unsigned int n)
{
	int i;
	u16 *ss = (u16 *) s;

	for (i = 0; i < n; i++) {
		ss[i] = ( u16 ) c;
	}
}

void video_init(void)
{
	static int inited=0;

	vidmem = (unsigned char *)phys_to_virt(VIDBUFFER);

	if (!inited) {
		video_line = 0;
		video_col = 0;
	
	 	memsetw(vidmem, VGA_ATTR_CLR_WHT, 2*1024); //

		inited=1;
	}
}

static void video_scroll(void)
{
	int i;

	memcpy(vidmem, vidmem + COLS * 2, (LINES - 1) * COLS * 2);
	for (i = (LINES - 1) * COLS * 2; i < LINES * COLS * 2; i += 2)
		vidmem[i] = ' ';
}

void vga_putc(unsigned char byte)
{
	if (byte == '\n') {
		video_line++;
		video_col = 0;

	} else if (byte == '\r') {
		video_col = 0;

	} else if (byte == '\b') {
		video_col--;

	} else if (byte == '\t') {
		video_col += 4;

	} else if (byte == '\a') {
		//beep
		//beep(500);

	} else {
		vidmem[((video_col + (video_line *COLS)) * 2)] = byte;
		vidmem[((video_col + (video_line *COLS)) * 2) +1] = VGA_ATTR_CLR_WHT;
		video_col++;
	}
	if (video_col < 0) {
		video_col = 0;
	}
	if (video_col >= COLS) {
		video_line++;
		video_col = 0;
	}
	if (video_line >= LINES) {
		video_scroll();
		video_line--;
	}
	// move the cursor
	write_crtc((video_col + (video_line *COLS)) >> 8, CRTC_CURSOR_HI);
	write_crtc((video_col + (video_line *COLS)) & 0x0ff, CRTC_CURSOR_LO);
}

#endif

