#ifndef ETHERBOOT_BITS_STRING_H
#define ETHERBOOT_BITS_STRING_H

/* define inline optimized string functions here */

#define	__HAVE_ARCH_MEMCPY
//extern void * memcpy(const void *d, const void *s, size_t count);

#define __HAVE_ARCH_MEMCMP
//extern int memcmp(const void * s ,const void * d ,size_t );

#define __HAVE_ARCH_MEMSET
//extern void * memset(const void * s, int c, size_t count);

#define __HAVE_ARCH_MEMMOVE
static inline void *memmove(void *s1, const void *s2, size_t n) {

   unsigned int i;
   char *tmp = s1;
   char *cs2 = (char *) s2;

   if (tmp < cs2) {
      for(i=0; i<n; ++i, ++tmp, ++cs2)
           *tmp =  *cs2;
   }
   else {
      tmp += n - 1;
      cs2 += n - 1;
      for(i=0; i<n; ++i, --tmp, --cs2)
              *tmp = *cs2;
   }
   return(s1);
}

#endif /* ETHERBOOT_BITS_STRING_H */
