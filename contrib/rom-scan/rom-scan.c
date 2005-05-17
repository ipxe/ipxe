#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#ifndef	__TURBOC__
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef	__TURBOC__
#define	HUGE	huge
#else
#define	HUGE
#endif

#define ROMSTART     0xC8000
#define ROMEND       0xE8000
#define ROMINCREMENT 0x00800
#define ROMMASK      0x03FFF

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)(long long)-1)
#endif

typedef struct Images {
  struct Images *next;
  long           start;
  long           size;
} Images;

static void rom_scan(const unsigned char HUGE *rom,long offset,long len)
{
  static Images *images = NULL;
  Images *ptr;
/* The assignments to dummy are to overcome a bug in TurboC */
  long dummy, size;
  int chksum;
  long i;
  
  if (rom[offset] != 0x55 || rom[dummy = offset+1] != 0xAA)
    return;
  size = (long)rom[dummy = offset+2]*512L;
  printf("Found ROM header at %04lX:0000; "
	 "announces %ldk image (27C%02d EPROM)\n",
	 offset/16,(size+512)/1024,
	 size <=  1024 ?   8 :
	 size <=  2048 ?  16 :
	 size <=  4096 ?  32 :
	 size <=  8192 ?  64 :
	 size <= 16384 ? 128 :
	 size <= 32768 ? 256 :
	 size <= 65536 ? 512 : 11);
  if (offset & ROMMASK)
    printf("  This is a unusual position; not all BIOSs might find it.\n"
	   "   Try to move to a 16kB boundary.\n");
  if (size > len) {
    printf("  This image extends beyond %04X:0000. "
	   "It clashes with the system BIOS\n",
	   ROMEND/16);
    size = len; }
  for (chksum=0, i = size; i--; chksum += rom[dummy = offset+i]);
  if (chksum % 256)
    printf("  Checksum does not match. This image is not active\n");
  ptr        = malloc(sizeof(Images));
  ptr->next  = images;
  ptr->start = offset;
  ptr->size  = size;
  images     = ptr;
  for (ptr = ptr->next; ptr != NULL; ptr = ptr->next) {
    for (i = 0; i < size && i < ptr->size; i++)
      if (rom[dummy = ptr->start+i] != rom[dummy = offset+i])
	break;
    if (i > 32) {
      printf("   Image is identical with image at %04lX:0000 "
	     "for the first %ld bytes\n",
	     ptr->start/16,i);
      if (i >= 1024)
	if (i == size)
	  printf("    this means that you misconfigured the EPROM size!\n");
	else
	  printf("    this could suggest that you misconfigured the "
		 "EPROM size\n");
      else
	printf("    this is probably harmless. Just ignore it...\n"); } }
  return;
}

int main(void)
{
  long  i;
  unsigned char HUGE *rom;

#ifndef	__TURBOC__
  int	fh;
  if ((fh = open("/dev/kmem",O_RDONLY|O_SYNC)) < 0) {
    fprintf(stderr,"Could not open \"/dev/kmem\": %s\n",0 );//strerror(errno));
    return(1); }
  if ((rom = mmap(NULL,ROMEND-ROMSTART,PROT_READ,MAP_SHARED,fh,
		  ROMSTART)) == MAP_FAILED) {
    fprintf(stderr,"Could not mmap \"/dev/kmem\": %s\n",0); //strerror(errno));
    close(fh);
    return(1); }
  close(fh);
#endif
  for (i = ROMEND; (i -= ROMINCREMENT) >= ROMSTART; )
#ifdef	__TURBOC__
    rom_scan(0,i,ROMEND-i);
#else
    rom_scan(rom-ROMSTART,i,ROMEND-i);
  munmap(rom,ROMEND-ROMSTART);
#endif
  return(0);
}
