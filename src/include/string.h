/*
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2004 Tobias Lorenz
 *
 *  string handling functions
 *  based on linux/include/linux/ctype.h
 *       and linux/include/linux/string.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef ETHERBOOT_STRING_H
#define ETHERBOOT_STRING_H

#include "bits/string.h"


/* *** FROM ctype.h *** */

#define isdigit(c)	((c & 0x04) != 0)
#define islower(c)	((c & 0x02) != 0)
//#define isspace(c)	((c & 0x20) != 0)
#define isupper(c)	((c & 0x01) != 0)

static inline unsigned char tolower(unsigned char c)
{
	if (isupper(c))
		c -= 'A'-'a';
	return c;
}

static inline unsigned char toupper(unsigned char c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}


/* *** FROM string.h *** */

int strnicmp(const char *s1, const char *s2, size_t len);
char * strcpy(char * dest,const char *src);
char * strncpy(char * dest,const char *src,size_t count);
char * strcat(char * dest, const char * src);
char * strncat(char *dest, const char *src, size_t count);
int strcmp(const char * cs,const char * ct);
int strncmp(const char * cs,const char * ct,size_t count);
char * strchr(const char * s, int c);
char * strrchr(const char * s, int c);
size_t strlen(const char * s);
size_t strnlen(const char * s, size_t count);
size_t strspn(const char *s, const char *accept);
char * strpbrk(const char * cs,const char * ct);
char * strtok(char * s,const char * ct);
char * strsep(char **s, const char *ct);
void * memset(void * s,int c,size_t count);
char * bcopy(const char * src, char * dest, int count);
void * memcpy(void * dest,const void *src,size_t count);
void * memmove(void * dest,const void *src,size_t count);
int memcmp(const void * cs,const void * ct,size_t count);
void * memscan(void * addr, int c, size_t size);
char * strstr(const char * s1,const char * s2);
void * memchr(const void *s, int c, size_t n);

#endif /* ETHERBOOT_STRING */
