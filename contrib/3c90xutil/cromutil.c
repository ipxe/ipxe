/* 
 * 3c905cutil.c - perform various control ops on the 3C905C bios rom
 *             which we assume to be an AT49BV512
 *
 */

#ifndef __i386__
#  error "This program can't compile or run on non-intel computers"
#else

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>

int main(int argc, char **argv)
{
  unsigned int ioaddr, i, n;
  unsigned char b;

  setuid(0); /* if we're setuid, do it really */
  if (argc != 3) {
    printf("Usage: romid ioaddr [erase|id|read >file|prog <file]\n");
    exit(-1);
  }
  if (iopl(3)) {
    perror("iopl()");
    exit(1);
  }
  sscanf(argv[1],"%x",&ioaddr);

  /* Set the register window to 0 for the 3C905C */
  outw(0x800, ioaddr+0xe);

  if (strcmp(argv[2], "erase") == 0) {
    /* do the funky chicken to erase the rom contents */
    outl(0x5555, ioaddr+0x4);
    outb(0xaa, ioaddr+0x8);
    outl(0x2aaa, ioaddr+0x4);
    outb(0x55, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4);
    outb(0x80, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4);
    outb(0xaa, ioaddr+0x8);
    outl(0x2aaa, ioaddr+0x4);
    outb(0x55, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4);
    outb(0x10, ioaddr+0x8);
    sleep (1);
    printf("Bios ROM at %04x has been erased\n", ioaddr);
  } else if (strcmp(argv[2], "id") == 0) {
    outl(0x5555, ioaddr+0x4);
    outb(0xaa, ioaddr+0x8);
    outl(0x2aaa, ioaddr+0x4);
    outb(0x55, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4);
    outb(0x90, ioaddr+0x8);
    /* 10ms delay needed */
    printf("Manufacturer ID - ");
    /* manuf. id */
    outl(0x0000, ioaddr+0x4);
    printf("%02x\n", inb(ioaddr+0x8));
    /* device id */
    outl(0x0001, ioaddr+0x4);
    printf("Device ID - %02x\n", inb(ioaddr+0x8));
    /* undo the funky chicken */
    outl(0x5555, ioaddr+0x4);
    outb(0xaa, ioaddr+0x8);
    outl(0x2aaa, ioaddr+0x4);
    outb(0x55, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4);
    outb(0xf0, ioaddr+0x8);
  } else if (strcmp(argv[2], "read") == 0) {
    for (i = 0; i < 65536; i++) {
      outl(i, ioaddr+0x4);
      b = inb(ioaddr+0x8);
      write(1, &b, 1);
    }
  } else if (strcmp(argv[2], "prog") == 0) {
    for (i = 0; i < 65536; i++) {
      n = read(0, &b, 1);
      if (n == 0)
	break;
      if (n < 0) {
	perror("File Error");
	exit(-3);
      }
      outl(0x5555, ioaddr+0x4);
      outb(0xaa, ioaddr+0x8);
      outl(0x2aaa, ioaddr+0x4);
      outb(0x55, ioaddr+0x8);
      outl(0x5555, ioaddr+0x4);
      outb(0xA0, ioaddr+0x8);
      outl(i, ioaddr+0x4);
      outb(b, ioaddr+0x8);
      while (inb(ioaddr+0x8) != b)
	;
    }
  }
  return 0;
}

#endif /* __i386__ */
