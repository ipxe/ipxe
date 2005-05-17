/*
   DP83820 flash utility written by Dave Ashley for NXTV, Inc.
   Copyright (C) 2004 by NXTV, Inc.
   Written 20040219 by Dave Ashley.

   Currently only supports the AT29C512

   This code is released under the terms of the GPL. No warranty.


   THEORY:
   This code uses the /proc/dp83820 file which is created by the
   dp83820flash.o module. That file allows single byte reads + writes
   to the bootrom.

*/

#include <unistd.h>
#include <sys/io.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>


// SMC9462TX card has D5 + D6 on the bootrom socket reversed
int fixb(int val)
{
	return (val&~0x60) | ((val&0x20)<<1) | ((val&0x40)>>1);
}
int openit(void)
{
int fd;
	fd=open("/proc/dp83820",O_RDWR);
	if(fd<0)
	{
		printf("Failed to open the /proc/dp83820 file to access the flashrom.\n");
		printf("Make sure you've done:\n");
		printf("  modprobe dp83820flash\n");
		exit(-1);
	}
	return fd;
}
void set(int addr, unsigned char val)
{
unsigned char msg[3];
int fd;
	fd=openit();
	msg[0]=addr;
	msg[1]=addr>>8;
	msg[2]=val;
	write(fd,msg,3);
	close(fd);
}
int get(int addr)
{
unsigned char msg[2];
int fd;
	fd=openit();
	msg[0]=addr;
	msg[1]=addr>>8;
	write(fd,msg,2);
	read(fd,msg,1);
	close(fd);
	return msg[0];
}


int getromsize(unsigned char *id)
{
	if(id[0]==0xbf && id[1]==0xb6) return 0x40000;
	if(id[0]==0xc2 && id[1]==0xb0) return 0x40000;
	if(id[0]==0x1f && id[1]==0x3d) return 0x10000;
	return -1;
}

#define MAXROMSIZE 0x200000
unsigned char *buffer;

int loadfile(char *name)
{
int filefd;
int filesize;
	filefd=open(name,O_RDONLY);
	if(filefd<0)
	{
		printf("Couldn't open file %s\n",name);
		return -1;
	}
	filesize=read(filefd,buffer,MAXROMSIZE);
	close(filefd);
	if(filesize<0)
	{
		printf("Error trying to read from file %s\n",name);
	}
	return filesize;
}

void readbios(char *name,int len)
{
int filefd;
int filesize=0;
unsigned char block[256];
int i,j;

	filefd=open(name,O_WRONLY|O_TRUNC|O_CREAT,0644);
	if(filefd<0)
	{
		printf("Couldn't create file %s for writing\n",name);
		return;
	}
	for(i=j=0;i<len;++i)
	{
		block[j++]=get(i);
		if(j<sizeof(block)) continue;
		filesize+=write(filefd,block,j);
		j=0;
	}
	close(filefd);
	if(filesize!=len)
	{
		printf("Error during write of %s file\n",name);
		return;
	}
	printf("BIOS contents saved to %s, $%x bytes\n",name,len);
}

int verifybios(char *name,int len, int print)
{
int filelen;
int i;
int same=0;

	filelen=loadfile(name);
	for(i=0;i<filelen;++i)
		if(get(i)!=buffer[i]) break;
	if(i<filelen)
	{
		if(print)
			printf("BIOS contents does not match file %s, from byte $%x\n",
				name,i);
	} else
	{
		if(print)
			printf("BIOS contents match file %s for all of its $%x bytes\n",
				name,i);
		same=1;
	}
	return same;
}

void writebios(char *name,int len,unsigned char *id)
{
int i;
int p1,p2;
int sectorsize=128;

	if(len!=loadfile(name))
	{
		printf("File size does not match expected ROM size\n");
		return;
	}
	if(0 && (id[0]!=0xbf || id[1]!=0xb6))
	{
		printf("Don't know how to write this kind of flash device\n");
		return;
	}

	printf("Erasing device\n");
	set(0x5555,fixb(0xaa));
	set(0x2aaa,fixb(0x55));
	set(0x5555,fixb(0x80));
	set(0x5555,fixb(0xaa));
	set(0x2aaa,fixb(0x55));
	set(0x5555,fixb(0x10));

	for(;;)
	{
		printf(".");fflush(stdout);
		usleep(250000);
		if(get(0)==get(0) && get(0)==get(0))
			break;
	}
	printf("BIOS erased\n");

	printf("Writing to BIOS\n");
	p1=-1;
	for(i=0;i<len;++i)
	{
		p2=100*i/(len-1);
		if(p2!=p1)
		{
			printf("\r%d%%",p1=p2);
			fflush(stdout);
		}
		if(i%sectorsize==0)
		{
			set(0x5555,fixb(0xaa));
			set(0x2aaa,fixb(0x55));
			set(0x5555,fixb(0xa0));
		}
		set(i,buffer[i]);
		if(i%sectorsize==sectorsize-1)
			while(get(0)!=get(0) || get(0)!=get(0));
	}
	printf("\n");
}

void helptext(char *name)
{
	printf("USE: %s <options>\n",name);
	printf("  -v <filename>  = verify bios rom contents with file\n");
	printf("  -w <filename>  = write to bios rom contents from file\n");
	printf("  -r <filename>  = read from bios rom contents to file\n");
	printf("  -f             = force erase/write even if contents already match\n");
	exit(0);
}

int main(int argc,char **argv)
{
int i;
int vals;
unsigned char id[4];
char *filename=0;
char action=0;
int romsize;
int force=0;
int same;

	vals=0;

	if(argc<2) helptext(argv[0]);
	for(i=1;i<argc;++i)
	{
		if(argv[i][0]!='-')
			helptext(argv[0]);
		switch(argv[i][1])
		{
		case 'f':
			force=1;
			break;
		case 'v':
		case 'w':
		case 'r':
			action=argv[i][1];
			if(i+1<argc)
				filename=argv[++i];
			else helptext(argv[0]);
			break;
		default:
			helptext(argv[0]);
		}
		
	}

	buffer=malloc(MAXROMSIZE);
	if(!buffer)
	{
		printf("No memory available!\n");
		exit(-1);
	}

	set(0x5555,fixb(0xaa)); // get into flash ID mode
	set(0x2aaa,fixb(0x55));
	set(0x5555,fixb(0x90));

	for(i=0;i<4;++i) id[i]=get(i);

	set(0x5555,fixb(0xaa)); // get out of flash ID mode
	set(0x2aaa,fixb(0x55));
	set(0x5555,fixb(0xf0));
	usleep(10000);

	for(i=0;i<4;++i)
		if(id[i]!=get(i)) break;
	if(i==4)
	{
		printf("Could not read BIOS flashrom ID.\n");
		goto biosdone;
	}
	printf("ID %02x %02x\n",id[0],id[1]);
	romsize=getromsize(id);
	if(romsize<0)
	{
		printf("Unknown rom type\n");
		goto biosdone;
	}
	printf("romsize=$%x bytes\n",romsize);
	if(action=='r')
		readbios(filename,romsize);
	if(action=='w')
	{
		if(!force)
			same=verifybios(filename,romsize,0);
		else
			same=0;
		if(!same)
			writebios(filename,romsize,id);
	}
	if(action=='v' || action=='w')
		verifybios(filename,romsize,1);

biosdone:

	return 0;
}
