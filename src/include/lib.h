#ifndef LIB_H
#define LIB_H

#include <stdint.h>

int getline(char *buf, int max);

extern struct pci_device *dev_list;
extern int n_devs;

extern void pci_init(void);
extern struct pci_device *pci_find_device(int vendor, int device, int devclass,
int prog_if, int index);

void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

char *strdup(const char *s);

int isspace(int c);

unsigned long long simple_strtoull(const char *cp,char **endp,unsigned int base);
unsigned long long strtoull_with_suffix(const char *cp,char **endp,unsigned int base);

unsigned int get_le32(const unsigned char *);
unsigned int get_le16(const unsigned char *);
void hexdump(const void *p, unsigned int len);

long long simple_strtoll(const char *cp,char **endp,unsigned int base);

#define LOADER_NOT_SUPPORT 0xbadf11e

struct sys_info;
int elf_load(struct sys_info *, const char *filename, const char *cmdline);

#if LINUX_LOADER
int linux_load(struct sys_info *, const char *filename, const char *cmdline);
#else
#define linux_load(x,y,z) LOADER_NOT_SUPPORT /* nop */
#endif

#endif /* LIB_H */
