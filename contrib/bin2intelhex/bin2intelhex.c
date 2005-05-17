/* name : bin2intelhex.c
 * from : Jean Marc Lacroix <jeanmarc.lacroix@free.fr>
 * date : 06/12/1997.
 * abstract : Y have rewrite this program from ????? with some modifications
 * to add :
 * - the Intel specification.
 * - correct a bug because my prom programmer don't understand the
 * initial format. Y suspect a bug in the calcul of the lrc
 * in the original program.
 * - correct the format of printf . In the original program, it was
 *   %x, and it is in fact %X, because in the Intel Format, all the
 * char are in upper case.
 * - correct the lrc calculation.
 * usage:
 *-------
 * this program read the standard input and put to the standard output
 * the result of the conversion.
 * an example of use :
 * cat my_bin | bin2intelhex > my_bin.hex or.....
 * bin2intelhex < my_bin > my_bin.hex
 */


/*
 * $Id$
 * $Log$
 * Revision 1.1.1.1  2005/05/17 16:45:06  mcb30
 * Import from Etherboot 5.4
 *
 * Revision 1.9  1997/12/14 05:14:54  install
 * - some documentation....
 *
 */

#include <stdio.h>
#include <unistd.h>

/* Intel Hex format specifications

The 8-bit Intel Hex File Format is a printable ASCII format consisting of one
 or more data records followed by an end of file record. Each
record consists of one line of information. Data records may appear in any
 order. Address and data values are represented as 2 or 4 hexadecimal
digit values. 

Record Format 
:LLAAAARRDDDD......DDDDCC 


LL
AAAA
RR
DD
CC
Length field. Number of data bytes.
Address field. Address of first byte.
Record type field. 00 for data and 01 for end of record.
Data field.
Checksum field. One's complement of length, address, record type and data
 fields modulo 256. 
CC = LL + AAAA + RR + all DD = 0

Example: 
:06010000010203040506E4 
:00000001FF 

The first line in the above example Intel Hex file is a data record addressed
 at location 100H with data values 1 to 6. The second line is the end
of file record, so that the LL field is 0

*/


typedef unsigned char t_u8;
typedef unsigned short t_u16;
/*
 * the choice for the total length (16) of a line, but the specification
 * can support an another value
 */
#define LL_MAX_LINE 16
typedef struct 
{ 
  t_u8 intel_lg_data;
  t_u16 intel_adr;
  t_u8 intel_type;
  t_u8 intel_data [LL_MAX_LINE];
  t_u8 intel_lrc;
} t_one_line;
#define INTEL_DATA_TYPE 0
#define EXIT_OK 0
int main (const int argc, const char ** const argv)
{
  t_one_line line;
  /*
   * init for the adress, please note that it is assume that the program begin at 0
   */
  line.intel_adr = 0;
  line.intel_type = INTEL_DATA_TYPE;
  /*
   * read the data on the standard input
   */
  while ((line.intel_lg_data = read (0, &line.intel_data [0] ,LL_MAX_LINE )) > 0) 
    {
      t_u8 i; 
      /*
       * and now for this line, calculate the lrc.
       */
      line.intel_lrc = line.intel_lg_data;
      line.intel_lrc += ((line.intel_adr >> 8) & 0xff);
      line.intel_lrc += (line.intel_adr &0xff);
      line.intel_lrc += line.intel_type;
      /*
       * the structure is ready, print it to stdout in the
       * right format
       */
      (void) printf (":%02X%04X%02X",
		     line.intel_lg_data,
		     line.intel_adr,
		     line.intel_type);
      /*
       * edit all the data read
       */
      for (i=0; i<line.intel_lg_data; i++)
	{
	  (void) printf ("%02X",
			 (line.intel_data [i] & 0xff));	  
	  /*
	   * add to the lrc the data print
	   */
	  line.intel_lrc +=line.intel_data [i];
	}
      /*
       * edit the value of the lrc and new line for the next
       */
      (void) printf ("%02X\n",
			 (0x100 - line.intel_lrc) & 0xff);
      /* 
       * prepare the new adress for the next line
       */
      line.intel_adr+=line.intel_lg_data;     
    }
  /*
   * print the last line with a length of 0 data, so that the lrc is easy to
   * calculate (ff+01 =0)
   */
  printf (":00000001FF\n");
  exit (EXIT_OK); 
}
