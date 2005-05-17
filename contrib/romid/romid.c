/* This little program is my try to provide information about the
   EtherBoot rom via environment variables to a batch file. This
   could be done better, I think, but it works...
   The program compiles with Borland C 3.1; other versions not tested.
   The C code for setting the environment variables I got from an
   archive, it was written by Richard Marks <rmarks@KSP.unisys.COM>.
   ROMID is written by Guenter Knauf <eflash@gmx.net>
*/
#define VERSION "0.6"
#define VDATE "2003-08-24"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROMSTART     0xC8000
#define ROMEND       0xE8000
#define ROMINCREMENT 0x00800
#define ROMMASK      0x03FFF

int verbose = 0;

int settheenv(char *symbol, char *val);

static int rom_scan(const unsigned char huge *rom,long offset,long len) {
  long size,i,j;
  char symbol[16];
  char val[64];
  char romid[64];
  char *rptr;

  if (rom[offset] != 0x55 || rom[offset+1] != 0xAA)
    return 0;

  size = (long)rom[offset+2]*512L;
  if (verbose) {
    printf("Found ROM header at %04lX:0000; announces %ldk image\n", offset/16,(size+512)/1024);
    if (offset & ROMMASK)
      printf("  This is a unusual position; not all BIOSs might find it.\n"
                        "   Try to move to a 16kB boundary.\n");
    if (size > len) {
      printf("  This image extends beyond %04X:0000. It clashes with the system BIOS\n", ROMEND/16);
      size = len;
    }
  }

  for (i=0; i<64; i++) {
    if (rom[offset+size-3-i] == 0xff)
      break;
  }
  if (20<i && i<63) {
    i--;
    for (j=0; j<i; j++)
      val[j] = rom[offset+size-3-i+j];
    val[i] = 0;
  } else
    return 0;

  if (strstr(val, "therboot") == NULL)
    return 0;

  if (verbose)
    printf("ROM Signature '%s'\n", val);
  if ((rptr = strstr(val, "rom")) != NULL) {
    for (i=1; i<4; i++) {
      rptr--;
      if (rptr[0] == 0x2E)
        break;
    }
    i = 0;
    while (!(rptr[0] == 0x20 || rptr < val)) {
      i++;
      rptr--;
    }
    rptr++;
    i--;
    strncpy(romid, rptr, i);
    romid[i] = 0;
    if (verbose)
      printf("ROM Driver ID '%s'\n", romid);
    strcpy(symbol, "ROMID");
    if (settheenv(symbol, romid))
      printf("Error setting evironment var %s with value %s\n", symbol, romid);
  } else {
    if (verbose)
      printf("Couldnt find driver name!\n");
    return 0;
  }
  if (rom[offset+0x1C] == 'P' && rom[offset+0x1D] == 'C' && rom[offset+0x1E] == 'I') {
    sprintf(val, "%02X%02X:%02X%02X", rom[offset+0x21], rom[offset+0x20],
                        rom[offset+0x23], rom[offset+0x22]);
    if (verbose)
      printf("ROM Vendor ID '%s'\n", val);
    strcpy(symbol, "PCIID");
    if (settheenv(symbol, val))
      printf("Error setting evironment var %s with value %s\n", symbol, val);
  }
  return 1;
}

/* **************** main stuff **************** */
int main (int argc, char *argv[]) {
  long  i;

  printf("\nROM-ID for Etherboot v%s (c) G. Knauf %s\n", VERSION, VDATE);
  if (argc > 1) {
    /* parse input parameters */
    for (argc--, argv++; *argv; argc--, argv++) {
      if ((strnicmp (*argv, "-", 1) == 0) || (strnicmp (*argv, "/", 1) == 0)) {
        if ((strnicmp (*argv, "-V", 2) == 0) || (strnicmp (*argv, "/V", 2) == 0)) {
          verbose = 1;
        } else {
          printf("Usage: %s [-v]\n");
        }
      }
    }
  }
  for (i = ROMEND; (i -= ROMINCREMENT) >= ROMSTART;)
    if (rom_scan(0,i,ROMEND-i))
      break;

  return 0;
}
