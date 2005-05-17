/* 
 * readutil.c - perform various control ops on the 3c509b bios rom
 *
 */

#ifndef __i386__
#  error "This program can't compile or run on non-intel computers"
#else

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __FreeBSD__

#include <fcntl.h>
#include <machine/cpufunc.h>

#define OUTB(data, port) 	outb(port, data)
#define OUTW(data, port) 	outw(port, data)
#define OUTL(data, port) 	outl(port, data)

#else

#include <sys/io.h>

#define OUTB(data, port) 	outb(data, port)
#define OUTW(data, port) 	outw(data, port)
#define OUTL(data, port) 	outl(data, port)

#endif

int main(int argc, char **argv)
{
    unsigned int i, j, n;
    unsigned int ioaddr;
    unsigned long recvrstat;
    unsigned char buf[128];
    unsigned char b;

    if (argc != 3) {
      printf("Usage: romid ioaddr [erase|protect|unprotect|id|read >file|prog <file]\n");
      exit(-1);
    }

#ifdef __FreeBSD__
    /* get permissions for in/out{blw} */
    open("/dev/io",O_RDONLY,0);
#else
    setuid(0); /* if we're setuid, do it really */
    if (iopl(3)) {
      perror("iopl()");
      exit(1);
    }
#endif

    sscanf(argv[1],"%x",&ioaddr);
    /* Set the register window to 3 for the 3c905b */
    OUTW(0x803, ioaddr+0xe);
    recvrstat = inl(ioaddr);	/* save the receiver status */
    /* set the receiver type to MII so the full bios rom address space
       can be accessed */
    OUTL((recvrstat & 0xf00fffff)|0x00600000, ioaddr);

    /* Set the register window to 0 for the 3c905b */
    OUTW(0x800, ioaddr+0xe);

    if (strcmp(argv[2], "erase") == 0) {
      /* do the funky chicken to erase the rom contents */
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xaa, ioaddr+0x8);
      OUTL(0x2aaa, ioaddr+0x4);
      OUTB(0x55, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0x80, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xaa, ioaddr+0x8);
      OUTL(0x2aaa, ioaddr+0x4);
      OUTB(0x55, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0x10, ioaddr+0x8);
      printf("Bios ROM at %04x has been erased\n", ioaddr);
    } else if (strcmp(argv[2], "protect") == 0) {
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xaa, ioaddr+0x8);
      OUTL(0x2aaa, ioaddr+0x4);
      OUTB(0x55, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xa0, ioaddr+0x8);
      printf("Software Data Protection for Bios ROM at %04x has been enabled\n",
	     ioaddr);
    } else if (strcmp(argv[2], "unprotect") == 0) {
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xaa, ioaddr+0x8);
      OUTL(0x2aaa, ioaddr+0x4);
      OUTB(0x55, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0x80, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xaa, ioaddr+0x8);
      OUTL(0x2aaa, ioaddr+0x4);
      OUTB(0x55, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0x20, ioaddr+0x8);
      printf("Software Data Protection for Bios ROM at %04x has been disabled\n",
	     ioaddr);
    } else if (strcmp(argv[2], "id") == 0) {
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xaa, ioaddr+0x8);
      OUTL(0x2aaa, ioaddr+0x4);
      OUTB(0x55, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0x90, ioaddr+0x8);
      /* 10ms delay needed */
      printf("Manufacturer ID - ");
      /* manuf. id */
      OUTL(0x0000, ioaddr+0x4);
      printf("%02x\n", inb(ioaddr+0x8));
      /* device id */
      OUTL(0x0001, ioaddr+0x4);
      printf("Device ID - %02x\n", inb(ioaddr+0x8));
      /* undo the funky chicken */
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xaa, ioaddr+0x8);
      OUTL(0x2aaa, ioaddr+0x4);
      OUTB(0x55, ioaddr+0x8);
      OUTL(0x5555, ioaddr+0x4);
      OUTB(0xf0, ioaddr+0x8);
    } else if (strcmp(argv[2], "read") == 0) {
      for (i = 0; i < 65536; i++) {
	OUTL(i, ioaddr+0x4);
	b = inb(ioaddr+0x8);
	write(1, &b, 1);
      }
    } else if (strcmp(argv[2], "prog") == 0) {
      /* program the rom in 128 bute chunks */
      for (i = 0, n = 0; i < 65536; i += n) {
	n = read(0, buf, 128);
	if (n == 0)
	  break;
	if (n < 0) {
	  perror("File Error");
	  exit(-3);
	}
	/* disable SDP temporarily for programming a sector */
	OUTL(0x5555, ioaddr+0x4);
	OUTB(0xaa, ioaddr+0x8);
	OUTL(0x2aaa, ioaddr+0x4);
	OUTB(0x55, ioaddr+0x8);
	OUTL(0x5555, ioaddr+0x4);
	OUTB(0xa0, ioaddr+0x8);
	for (j = 0; j < n; j++) {
	  OUTL(i+j, ioaddr+0x4);
	  OUTB(buf[j], ioaddr+0x8);
	}
	/* wait for the programming of this sector to coomplete */
	while (inb(ioaddr+0x8) != buf[j-1])
	  ;
      }
    }

    /* Set the register window to 3 for the 3c905b */
    OUTW(0x803, ioaddr+0xe);
    /* restore the receiver status */
    OUTL(recvrstat, ioaddr);
    return 0;
}

#endif /* __i386__ */
