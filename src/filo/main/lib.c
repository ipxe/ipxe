
#include <etherboot.h>
#include <lib.h>

char *strdup(const char *s)
{
    size_t sz = strlen(s) + 1;
    char *d = allot(sz);
    memcpy(d, s, sz);
    return d;
}

int isspace(int c)
{
    switch (c) {
    case ' ': case '\f': case '\n':
    case '\r': case '\t': case '\v':
        return 1;
    default:
        return 0;
    }
}

unsigned int get_le32(const unsigned char *p)
{
    return ((unsigned int) p[0] << 0)
	| ((unsigned int) p[1] << 8)
	| ((unsigned int) p[2] << 16)
	| ((unsigned int) p[3] << 24);
}

unsigned int get_le16(const unsigned char *p)
{
    return ((unsigned int) p[0] << 0)
	| ((unsigned int) p[1] << 8);
}
#if (DEBUG_ALL || DEBUG_ELFBOOT || DEBUG_ELFNOTE || DEBUG_LINUXBIOS || \
	DEBUG_MALLOC || DEBUG_MULTIBOOT || DEBUG_SEGMENT || DEBUG_SYS_INFO ||\
	DEBUG_TIMER || DEBUG_BLOCKDEV || DEBUG_PCI || DEBUG_LINUXLOAD ||\
	DEBUG_IDE || DEBUG_ELTORITO)

// It is needed by debug for filo
void hexdump(const void *p, unsigned int len)
{
	int i;
	const unsigned char *q = p;

	for (i = 0; i < len; i++) {
	    if (i%16==0)
		printf("%04x: ", i);
	    printf("%02x%c", q[i], i%16==15 ? '\n' : i%8==7 ? '-' : ' ');
	}
	if (i%16 != 0)
	    putchar('\n');
}
#endif
