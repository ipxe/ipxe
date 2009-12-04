/*
 * JLdL 21Jun04.
 *
 * cromutil.c
 *
 * Perform various control operations on the flash EEPROM of
 * _ the 3COM models 3C905C or 3C905CX network cards, in order
 * _ to write a boot program such as Etherboot into it.
 *
 * This program is meant for the Linux operating system only,
 * _ and only for the i386 architecture.
 *
 * The flash EEPROM usually used in these cards is the AT49BV512
 * _ chip, which has 512 Kbit (64 KByte). Another possible chip,
 * _ which is equivalent to this one, is the SST39VF512.
 *
 * Added alternative read128 and prog128 commands for cards with
 * _ the SST29EE020 fast page-write (super-)flash EEPROM, which
 * _ has 2 Mbit (256 KByte), and which has to be programmed in
 * _ a 128-byte page mode. NOTE: it seems that the card can
 * _ address only the first half of the memory in this chip,
 * _ so only 128 Kbytes are actually available for use.
 *
 * Added a few informative messages and a detailed help message.
 *
 */

#ifndef __i386__
#  error "This program can't compile or run on non-Intel computers"
#else

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>
#include <string.h>

int main(int argc, char **argv)
{
  /* Counters. */
  unsigned int i, j, n;
  /* For ROM chips larger than 64 KB, a long integer
     _ is needed for the global byte counter. */
  unsigned long k;
  /* The I/O address of the card. */
  unsigned int ioaddr;
  /* Storage for a byte. */
  unsigned char b;
  /* Storage for a page. */
  unsigned char buf[128];

  /* Initialize a few things to avoid compiler warnings. */
  i=0; j=0; n=0; k=0;

  /* Verify the command-line parameters; write
     _ out an usage message if needed. */
  if (argc != 3) {
    /* Exactly 2 command line parameters are needed. */
    printf("Usage: ./cromutil ioaddr command [(>|<) file]\n");
    printf(" (try './cromutil 0x0000 help' for details)\n");
    exit(-1);
  }

  /* Set the UID to root if possible. */
  setuid(0);

  /* Get port-access permissions for in{blw}/out{blw}. */
  if (iopl(3)) {
    perror("iopl()");
    exit(1);
  }

  /* Pass the I/O address of the card to a variable. */
  sscanf(argv[1],"%x",&ioaddr);

  /* Set the register window to 0. */
  outw(0x800, ioaddr+0xe);

  /*
   * Execute the requested command.
   *
   * "id": get and write out the ID numbers.
   */
  if (strcmp(argv[2], "id") == 0) {
    /* Software ID entry command sequence. */
    outl(0x5555, ioaddr+0x4); outb(0xaa, ioaddr+0x8);
    outl(0x2aaa, ioaddr+0x4); outb(0x55, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4); outb(0x90, ioaddr+0x8);
    /* A 10 ms delay is needed. */
    usleep(10000);
    /* Get the manufacturer id. */
    outl(0x0000, ioaddr+0x4);
    printf("Manufacturer ID - %02x\n", inb(ioaddr+0x8));
    /* Get the device id. */
    outl(0x0001, ioaddr+0x4);
    printf("Device ID - %02x\n", inb(ioaddr+0x8));
    /* Software ID exit command sequence. */
    outl(0x5555, ioaddr+0x4); outb(0xaa, ioaddr+0x8);
    outl(0x2aaa, ioaddr+0x4); outb(0x55, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4); outb(0xf0, ioaddr+0x8);
  }
  /*
   * "read": read data from the 512 Kbit ROM.
   */
  else if (strcmp(argv[2], "read") == 0) {
    /* Loop over the whole ROM. */
    for (k = 0; k < 65536; k++) {
      outl(k, ioaddr+0x4);
      b = inb(ioaddr+0x8);
      write(1, &b, 1);
    }
    /* Write out an informative message. */
    perror("Read 65536 bytes from ROM");
  }
  /*
   * "read128": this alternative is for the 2 Mbit ROM.
   */
  else if (strcmp(argv[2], "read128") == 0) {
    /* Loop over the accessible part of the ROM. */
    for (k = 0; k < 131072; k++) {
      outl(k, ioaddr+0x4);
      b = inb(ioaddr+0x8);
      write(1, &b, 1);
    }
    /* Write out an informative message. */
    perror("Read 131072 bytes from ROM");
  }
  /*
   * "erase": erase the ROM contents.
   */
  else if (strcmp(argv[2], "erase") == 0) {
    /* Software chip-erase command sequence. */
    outl(0x5555, ioaddr+0x4); outb(0xaa, ioaddr+0x8);
    outl(0x2aaa, ioaddr+0x4); outb(0x55, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4); outb(0x80, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4); outb(0xaa, ioaddr+0x8);
    outl(0x2aaa, ioaddr+0x4); outb(0x55, ioaddr+0x8);
    outl(0x5555, ioaddr+0x4); outb(0x10, ioaddr+0x8);
    /* Wait a bit. */
    sleep(1);
    /* Write out an informative message. */
    printf("Bios ROM at %04x has been erased: Success\n", ioaddr);
  }
  /*
   * "prog": program the 512 Kbit ROM.
   */
  else if (strcmp(argv[2], "prog") == 0) {
    /* Loop over the bytes in pages, to
       _ allow for a progress report. */
    for (j = 0; j < 512; j++) {
      for (i = 0; i < 128; i++) {
	/* If this program is to run on a diskless node,
	   _ must read in the byte _before_ changing the
	   _ mode of the chip, or NFS may block. */
	n = read(0, &b, 1);
	/* At EOF exit the inner loop. */
	if (n == 0)
	  break;
	if (n < 0) {
	  perror("Input File Error");
	  exit(-3);
	}
	/* Disable SDP temporarily for programming a byte. */
	outl(0x5555, ioaddr+0x4); outb(0xaa, ioaddr+0x8);
	outl(0x2aaa, ioaddr+0x4); outb(0x55, ioaddr+0x8);
	outl(0x5555, ioaddr+0x4); outb(0xA0, ioaddr+0x8);
	/* Calculate the address of the byte. */
	k=i+128*j;
	/* Program this byte. */
	outl(k, ioaddr+0x4); outb(b, ioaddr+0x8);
	/* Wait for the programming of this byte to complete. */
	while (inb(ioaddr+0x8) != b)
	  ;
      }
      /* At EOF exit the outer loop. */
      if (n == 0)
	break;
      /* Write out a progress report. */
      printf("."); fflush(NULL);
    }
    /* Write out an informative message. */
    printf("\nWrote %ld bytes to ROM: Success\n", k);
  }
  /*
   * "prog128": this alternative is for the 2 Mbit ROM.
   */
  else if (strcmp(argv[2], "prog128") == 0) {
    /* Loop over the accessible pages; the card can
       _ access only the first half of the chip. */
    for (j = 0; j < 1024; j++) {
      /* If this program is to run on a diskless node,
	 _ must read in the page _before_ changing the
	 _ mode of the chip, or NFS may block. */
      n = read(0, buf, 128);
      /* At EOF exit the loop. */
      if (n == 0)
	break;
      if (n < 0) {
	perror("Input File Error");
	exit(-3);
      }
      /* Disable SDP temporarily for programming a page. */
      outl(0x5555, ioaddr+0x4); outb(0xaa, ioaddr+0x8);
      outl(0x2aaa, ioaddr+0x4); outb(0x55, ioaddr+0x8);
      outl(0x5555, ioaddr+0x4); outb(0xA0, ioaddr+0x8);
      /* Loop over the bytes in a page. */
      for (i = 0; i < n; i++) {
	/* Calculate the address of the byte. */
	k=i+128*j;
	/* Program this byte. */
	outl(k, ioaddr+0x4); outb(buf[i], ioaddr+0x8);
      }
      /* Wait for the programming of this page to complete. */
      while (inb(ioaddr+0x8) != buf[i-1])
	;
      /* Write out a progress report. */
      printf("."); fflush(NULL);
    }
    /* Write out an informative message. */
    printf("\nWrote %d pages to ROM: Success\n", j);
  }
  /*
   * "help": write out a detailed help message.
   */
  else if (strcmp(argv[2], "help") == 0) {
    printf("This utility can be used to write data, usually boot loaders\n");
    printf("  such as Etherboot, to the flash EEPROM of the 3COM models\n");
    printf("  3C905C and 3C905CX network cards. You use it like this:\n");
    printf("        ./cromutil ioaddr command [(>|<) file]\n");
    printf("Here ioaddr is the hexadecimal I/O address of the card, such\n");
    printf("  as 0xA123, in some cases you need input/output redirection\n");
    printf("  from/to a file, and the command can be one of these:\n");
    printf("  id               get the ID numbers of the card;\n");
    printf("  read > file      read the contents of the ROM into a file;\n");
    printf("  read128 > file   read the contents of the ROM into a file;\n");
    printf("  erase            erase the whole ROM to the 1 state;\n");
    printf("  prog < file      write the contents of a file into the ROM;\n");
    printf("  prog128 < file   write the contents of a file into the ROM.\n");
    printf("You can get the I/O address of the card using the commands\n");
    printf("  'lspci -v', 'cat /proc/pci', or 'dmesg | grep -i 3C905C'.\n");
    printf("The read and prog commands are to be used if the card has a\n");
    printf("  traditional 512 Kb (64 KB) flash EEPROM chip, such as:\n");
    printf("  | AT49BV512 | SST39VF512 |\n");
    printf("The read128 and prog128 versions are for cards with a 2 Mb\n");
    printf("  (128 KB usable) page-write flash EEPROM chip, such as:\n");
    printf("  | SST29EE020 |\n");
  }
  /*
   * Write out the usage message if an unknown command is used.
   */
  else {
    printf("Usage: ./cromutil ioaddr command [(>|<) file]\n");
    printf("(try './cromutil 0x0000 help' for details)\n");
    exit(-1);
  }
  return 0;
}

#endif /* __i386__ */
