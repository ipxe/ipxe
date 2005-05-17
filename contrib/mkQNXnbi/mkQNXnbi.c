//*****************************************************************************
//
//      Purpose:        Make a boot-image for EtherBoot
//
//
//      Compiler:       This source can be compiled with gcc and Watcom C
//
//
//      Note:           The QNX boot image can be build with any reasonable
//                      start address, e.g. 0x1000 (default) or 0x10000
//                      (widespread Boot-Rom address)
//
//
//      Author:         Anders Larsen
//
//
//      Copyright:      (C) 1999 by
//
//                      Anders Larsen
//                      systems engineer
//                      Gutleuthausstr. 3
//                      D-69469 Weinheim
//                      Germany
//                      phone:  +49-6201-961717
//                      fax:    +49-6201-961718
//                      e-mail: al@alarsen.net
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//-----------------------------------------------------------------------------
//
//      Change Log:
//        V0.2: Sun 1999-12-13 Anders Larsen <al@alarsen.net>
//*****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>


// max. size of QNX OS boot image is 512K
#define MAXSIZE (512*1024)

typedef unsigned short ushort_t;
typedef unsigned long  ulong_t;


// global header of tagged image:
struct initial_t
{
  ulong_t magic;
  ulong_t length;
  ulong_t location;
  ulong_t start;
};


// header of each image:
struct header_t
{
  ulong_t flags;
  ulong_t loadaddr;
  ulong_t imgsize;
  ulong_t memsize;
};


// global header of the QNX EtherBoot image:
struct qnx_loader_t
{
  struct initial_t setup;
  struct header_t  qnx;
};


// global header:
union
{
  struct qnx_loader_t h;
  char                filler[512];
} header;


char buffer[MAXSIZE];


int usage( char* const* argv )
{
  fprintf( stderr, "%s - make a tagged boot image for EtherBoot\n", *argv );
  fprintf( stderr, "\nuse:\n" );
  fprintf( stderr, "%s [ -<option> ]*\n", *argv );
  fprintf( stderr, "\noptions:\n" );
  fprintf( stderr, "  i <input file>  : QNX boot file     (default: stdin)\n" );
  fprintf( stderr, "  o <output file> : tagged image file (default: stdout)\n" );
  fprintf( stderr, "  v               : be verbose\n" );
  return EXIT_FAILURE;
}

#ifdef __USAGE
%C - make a tagged boot image for EtherBoot

use:
%C [ -<option> ]* 

options:
  i <input file>  : QNX boot file     (default: stdin)
  o <output file> : tagged image file (default: stdout)
  v               : be verbose
#endif


int main( int argc, char* const* argv )
{
  int ch, l;
  int verbose = 0;

  while ( ( ch = getopt( argc, argv, "hi:o:v" ) ) != EOF )
    switch ( ch )
    {
      case 'i':
        if ( !freopen( optarg, "r", stdin ) )
        {
          perror( "can't open input file" );
          return EXIT_FAILURE;
        }
        break;

      case 'o':
        if ( !freopen( optarg, "w", stdout ) )
        {
          perror( "can't create output file" );
          return EXIT_FAILURE;
        }
        break;

      case 'v':
        verbose++;
        break;

      case 'h':
      default:
        return usage( argv );
    }
  if ( optind != argc )
    return usage( argv );

  memset( &header, 0, sizeof header );
  header.h.setup.magic     = 0x1b031336;    // magic number
  header.h.setup.length    =          4;
  header.h.setup.location  = 0x93e00000;    // just below the EtherBoot rom
  header.h.setup.start     =          0;    // filled in dynamically
  header.h.qnx.flags       = 0x04000004;    // single image only
  header.h.qnx.loadaddr    =          0;    // filled in dynamically
  header.h.qnx.imgsize     =          0;    // filled in dynamically
  header.h.qnx.memsize     =          0;    // filled in dynamically

  // read the QNX image from stdin:
  for ( ; ( l = fread( buffer + header.h.qnx.imgsize, 1, 1024, stdin ) ) > 0;
        header.h.qnx.imgsize += l
      )
    ;
  header.h.qnx.memsize = header.h.qnx.imgsize;

  // fill in the real load-address of the QNX boot image:
  header.h.setup.start  = *(ushort_t*)&buffer[10] << 16;
  header.h.qnx.loadaddr = *(ushort_t*)&buffer[10] <<  4;

  // write the tagged image file to stdout:
  fwrite( &header, 1, 512, stdout );
  fwrite( buffer, 1, header.h.qnx.imgsize, stdout );

  if ( verbose )
  {
    // print diagnostic information:
    fprintf( stderr, "QNX image size: %d bytes (%dK), load addr: 0x%05X\n",
             header.h.qnx.imgsize,
             header.h.qnx.imgsize / 1024,
             header.h.qnx.loadaddr
           );
  }
  return EXIT_SUCCESS;
}
