/* Based on code example from libqrencode and modified to use IBM CP 437 chars */

#include "libqrencode/qrencode.h"
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#define margin 4

static const char glyphs[] =
	" "	/* White Space */
	"\xDC"	/* Low Block */
	"\xDF"	/* High Block */
	"\xDB";	/* Full Block */

static int writeANSI_margin(char* outbuff, size_t outbuff_sz, int width, int p_margin, const char *white, const char *reset )
{
	int y;
	int len = 0;

	for (y = 0; y < margin; y+=2 ) {
		len += snprintf( &outbuff[len], outbuff_sz - len, "%s", white); /* Initialize the color - default white */
		len += snprintf( &outbuff[len], outbuff_sz - len, "%*c", width + (p_margin*2), ' ');
		len += snprintf( &outbuff[len], outbuff_sz - len, "%s\n", reset); // reset to default colors for newline
	}
	return len;
}

static int writeANSI(const QRcode *qrcode, char *outbuff, size_t outbuff_sz)
{
	unsigned char *rowH;
	unsigned char *rowL;
	unsigned char *p;
	int x, y;
	int len = 0;

	const unsigned char *E = (const unsigned char *)"";

//	const char white[] = "\033[47m";
	const char white[] = "";
	const int  white_s = sizeof( white ) -1;
//	const char black[] = "\033[40m";
//	const char reset[] = "\033[0m";
	const char reset[] = "";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
	const size_t minLen  = (( (white_s) * 2 ) + ( (qrcode->width * 2) * qrcode->width                 )); // Unlikely
//	const size_t maxLen  = (( (white_s) * 2 ) + ( (qrcode->width * 2) * qrcode->width * (white_s + 1 ))); // Unlikely
//	const size_t typLen  = (minLen + maxLen)/2;	// More typical?
#pragma GCC diagnostic pop

	if ( outbuff_sz < minLen ) {
		snprintf(outbuff, outbuff_sz, "Insufficient buffer to render URL QR-Code.\n\tNeed at least %d bytes, only have %d\n", minLen, outbuff_sz);
	//	return( -1 ); // Error
	}

	/* top margin */
	len += writeANSI_margin(&outbuff[len], outbuff_sz-len, qrcode->width, margin, white, reset);
	/* data */
	p = qrcode->data;
	for(y = 0; y < qrcode->width; y+=2) {
		rowH = (p+((y+0)*qrcode->width));
		rowL = (p+((y+1)*qrcode->width));

		len += snprintf( &outbuff[len], outbuff_sz - len, "%s", white); /* Initialize the color - default white */
		for(x = 0; x < margin; x++ ){
			len += snprintf( &outbuff[len], outbuff_sz - len, "%s", " ");
		}

		for(x = 0; x < qrcode->width; x++) {
			len += snprintf( &outbuff[len], outbuff_sz - len, "%c", glyphs[
			 ( ((*(                    rowH+x  )&0x1)<<1) |
			   ((*((y+1)<qrcode->width?rowL+x:E)&0x1)<<0) )
			]);
		}

		for(x = 0; x < margin; x++ ){
			len += snprintf( &outbuff[len], outbuff_sz - len, "%s", " ");
		}
		len += snprintf( &outbuff[len], outbuff_sz - len, "%s\n", reset);
	}

	/* bottom margin */
	len += writeANSI_margin(&outbuff[len], outbuff_sz-len, qrcode->width, margin, white, reset);

	return len;
}

int uriqrencode(const char * URI, char *outbuff, size_t outbuff_sz)
{

	QRcode *qrcode = QRcode_encodeString(URI, 0, QR_ECLEVEL_L,
			QR_MODE_8, 1);

	outbuff_sz = writeANSI( qrcode, outbuff, outbuff_sz );

	QRcode_free(qrcode);
	return outbuff_sz;
}

//#define TEST_QRCODE

#ifdef TEST_QRCODE
int main(int argc, char *argv[])
{
	#define SQUARE 46

	char buffer[((SQUARE*2)*SQUARE)];
	char* arg;

	int len;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s string\n", argv[0]);

		arg = "https://youtu.be/Xe1o5JDwp2k";
	} else {
		arg = argv[1];
	}

	len = uriqrencode( arg, buffer, sizeof( buffer ) );

	if (len < 0) {
		fputs( "Error\n", stdout );
	}
	fputs( buffer, stdout );

	return 0;
}
#endif /* TEST_QRCODE */ 
