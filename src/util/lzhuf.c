/*
----------------------------------------------------------------------------

M. LZHuf Compression

This is the LZHuf compression algorithm as used in DPBOX and F6FBB.

----------------------------------------------------------------------------
*/
/**************************************************************
    lzhuf.c
    written by Haruyasu Yoshizaki 11/20/1988
    some minor changes 4/6/1989
    comments translated by Haruhiko Okumura 4/7/1989

    minor beautifications and adjustments for compiling under Linux
    by Markus Gutschke <gutschk@math.uni-muenster.de>
    						1997-01-27

    Modifications to allow use as a filter by  Ken Yap <ken_yap@users.sourceforge.net>.
						1997-07-01

    Small mod to cope with running on big-endian machines
    by Jim Hague <jim.hague@acm.org)
						1998-02-06

    Make compression statistics report shorter
    by Ken Yap <ken_yap@users.sourceforge.net>.
						2001-04-25
**************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifndef VERBOSE
#define Fprintf(x)
#define wterr     0
#else
#define Fprintf(x) fprintf x
#if defined(ENCODE) || defined(DECODE)
static char wterr[] = "Can't write.";
#ifdef ENCODE
static unsigned long int codesize = 0;
#endif
static unsigned long int printcount = 0;
#endif
#endif

#ifndef MAIN
extern
#endif
FILE  *infile, *outfile;

#if defined(ENCODE) || defined(DECODE)
static unsigned long int  textsize = 0;

static __inline__ void Error(char *message)
{
    Fprintf((stderr, "\n%s\n", message));
    exit(EXIT_FAILURE);
}

/* These will be a complete waste of time on a lo-endian */
/* system, but it only gets done once so WTF. */
static unsigned long i86ul_to_host(unsigned long ul)
{
    unsigned long res = 0;
    int i;
    union
    {
    	unsigned char c[4];
	unsigned long ul;
    } u;

    u.ul = ul;
    for (i = 3; i >= 0; i--)
    	res = (res << 8) + u.c[i];
    return res;
}

static unsigned long host_to_i86ul(unsigned long ul)
{
    int i;
    union
    {
    	unsigned char c[4];
	unsigned long ul;
    } u;

    for (i = 0; i < 4; i++)
    {
    	u.c[i] = ul & 0xff;
	ul >>= 8;
    }
    return u.ul;
}
#endif

/********** LZSS compression **********/

#define N       4096    /* buffer size */
/* Attention: When using this file for f6fbb-type compressed data exchange,
   set N to 2048 ! (DL8HBS) */
#define F       60  /* lookahead buffer size */
#define THRESHOLD   2
#define NIL     N   /* leaf of tree */

#if defined(ENCODE) || defined(DECODE)
static unsigned char
        text_buf[N + F - 1];
#endif

#ifdef ENCODE
static int     match_position, match_length,
               lson[N + 1], rson[N + 257], dad[N + 1];

static void InitTree(void)  /* initialize trees */
{
    int  i;

    for (i = N + 1; i <= N + 256; i++)
        rson[i] = NIL;          /* root */
    for (i = 0; i < N; i++)
        dad[i] = NIL;           /* node */
}

static void InsertNode(int r)  /* insert to tree */
{
    int  i, p, cmp;
    unsigned char  *key;
    unsigned c;

    cmp = 1;
    key = &text_buf[r];
    p = N + 1 + key[0];
    rson[r] = lson[r] = NIL;
    match_length = 0;
    for ( ; ; ) {
        if (cmp >= 0) {
            if (rson[p] != NIL)
                p = rson[p];
            else {
                rson[p] = r;
                dad[r] = p;
                return;
            }
        } else {
            if (lson[p] != NIL)
                p = lson[p];
            else {
                lson[p] = r;
                dad[r] = p;
                return;
            }
        }
        for (i = 1; i < F; i++)
            if ((cmp = key[i] - text_buf[p + i]) != 0)
                break;
        if (i > THRESHOLD) {
            if (i > match_length) {
                match_position = ((r - p) & (N - 1)) - 1;
                if ((match_length = i) >= F)
                    break;
            }
            if (i == match_length) {
                if ((c = ((r - p) & (N - 1)) - 1) < match_position) {
                    match_position = c;
                }
            }
        }
    }
    dad[r] = dad[p];
    lson[r] = lson[p];
    rson[r] = rson[p];
    dad[lson[p]] = r;
    dad[rson[p]] = r;
    if (rson[dad[p]] == p)
        rson[dad[p]] = r;
    else
        lson[dad[p]] = r;
    dad[p] = NIL;  /* remove p */
}

static void DeleteNode(int p)  /* remove from tree */
{
    int  q;

    if (dad[p] == NIL)
        return;         /* not registered */
    if (rson[p] == NIL)
        q = lson[p];
    else
    if (lson[p] == NIL)
        q = rson[p];
    else {
        q = lson[p];
        if (rson[q] != NIL) {
            do {
                q = rson[q];
            } while (rson[q] != NIL);
            rson[dad[q]] = lson[q];
            dad[lson[q]] = dad[q];
            lson[q] = lson[p];
            dad[lson[p]] = q;
        }
        rson[q] = rson[p];
        dad[rson[p]] = q;
    }
    dad[q] = dad[p];
    if (rson[dad[p]] == p)
        rson[dad[p]] = q;
    else
        lson[dad[p]] = q;
    dad[p] = NIL;
}
#endif

/* Huffman coding */

#define N_CHAR      (256 - THRESHOLD + F)
                /* kinds of characters (character code = 0..N_CHAR-1) */
#define T       (N_CHAR * 2 - 1)    /* size of table */
#define R       (T - 1)         /* position of root */
#define MAX_FREQ    0x8000      /* updates tree when the */
                    /* root frequency comes to this value. */
typedef unsigned char uchar;

/* table for encoding and decoding the upper 6 bits of position */

/* for encoding */

#ifdef ENCODE
static uchar p_len[64] = {
    0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

static uchar p_code[64] = {
    0x00, 0x20, 0x30, 0x40, 0x50, 0x58, 0x60, 0x68,
    0x70, 0x78, 0x80, 0x88, 0x90, 0x94, 0x98, 0x9C,
    0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC,
    0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE,
    0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE,
    0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEC, 0xEE,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};
#endif

#ifdef DECODE
/* for decoding */
static uchar d_code[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
    0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
    0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
    0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
    0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
    0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
    0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};

static uchar d_len[256] = {
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};
#endif

#if defined(ENCODE) || defined(DECODE)
static unsigned freq[T + 1];   /* frequency table */

static int prnt[T + N_CHAR];   /* pointers to parent nodes, except for the */
            /* elements [T..T + N_CHAR - 1] which are used to get */
            /* the positions of leaves corresponding to the codes. */

static int son[T];     /* pointers to child nodes (son[], son[] + 1) */
#endif

#ifdef DECODE
static unsigned getbuf = 0;
static uchar getlen = 0;

static int GetBit(void)    /* get one bit */
{
    int i;

    while (getlen <= 8) {
        if ((i = getc(infile)) < 0) i = 0;
        getbuf |= i << (8 - getlen);
        getlen += 8;
    }
    i = getbuf;
    getbuf <<= 1;
    getlen--;
    return ((signed short)i < 0);
}

static int GetByte(void)   /* get one byte */
{
    unsigned short i;

    while (getlen <= 8) {
        if ((signed short)(i = getc(infile)) < 0) i = 0;
        getbuf |= i << (8 - getlen);
        getlen += 8;
    }
    i = getbuf;
    getbuf <<= 8;
    getlen -= 8;
    return i >> 8;
}
#endif

#ifdef ENCODE
static unsigned putbuf = 0;
static uchar putlen = 0;

static void Putcode(int l, unsigned c)     /* output c bits of code */
{
    putbuf |= c >> putlen;
    if ((putlen += l) >= 8) {
        if (putc(putbuf >> 8, outfile) == EOF) {
            Error(wterr);
        }
        if ((putlen -= 8) >= 8) {
            if (putc(putbuf, outfile) == EOF) {
                Error(wterr);
            }
#ifdef VERBOSE
            codesize += 2;
#endif
            putlen -= 8;
            putbuf = c << (l - putlen);
        } else {
	  putbuf <<= 8;
#ifdef VERBOSE
	  codesize++;
#endif
        }
    }
}
#endif

/* initialization of tree */

#if defined(ENCODE) || defined(DECODE)
static void StartHuff(void)
{
    int i, j;

    for (i = 0; i < N_CHAR; i++) {
        freq[i] = 1;
        son[i] = i + T;
        prnt[i + T] = i;
    }
    i = 0; j = N_CHAR;
    while (j <= R) {
        freq[j] = freq[i] + freq[i + 1];
        son[j] = i;
        prnt[i] = prnt[i + 1] = j;
        i += 2; j++;
    }
    freq[T] = 0xffff;
    prnt[R] = 0;
}

/* reconstruction of tree */

static void reconst(void)
{
    int i, j, k;
    unsigned f, l;

    /* collect leaf nodes in the first half of the table */
    /* and replace the freq by (freq + 1) / 2. */
    j = 0;
    for (i = 0; i < T; i++) {
        if (son[i] >= T) {
            freq[j] = (freq[i] + 1) / 2;
            son[j] = son[i];
            j++;
        }
    }
    /* begin constructing tree by connecting sons */
    for (i = 0, j = N_CHAR; j < T; i += 2, j++) {
        k = i + 1;
        f = freq[j] = freq[i] + freq[k];
        for (k = j - 1; f < freq[k]; k--);
        k++;
        l = (j - k) * 2;
        memmove(&freq[k + 1], &freq[k], l);
        freq[k] = f;
        memmove(&son[k + 1], &son[k], l);
        son[k] = i;
    }
    /* connect prnt */
    for (i = 0; i < T; i++) {
        if ((k = son[i]) >= T) {
            prnt[k] = i;
        } else {
            prnt[k] = prnt[k + 1] = i;
        }
    }
}

/* increment frequency of given code by one, and update tree */

static void update(int c)
{
    int i, j, k, l;

    if (freq[R] == MAX_FREQ) {
        reconst();
    }
    c = prnt[c + T];
    do {
        k = ++freq[c];

        /* if the order is disturbed, exchange nodes */
        if (k > freq[l = c + 1]) {
            while (k > freq[++l]);
            l--;
            freq[c] = freq[l];
            freq[l] = k;

            i = son[c];
            prnt[i] = l;
            if (i < T) prnt[i + 1] = l;

            j = son[l];
            son[l] = i;

            prnt[j] = c;
            if (j < T) prnt[j + 1] = c;
            son[c] = j;

            c = l;
        }
    } while ((c = prnt[c]) != 0);   /* repeat up to root */
}
#endif

#ifdef ENCODE
#if 0
static unsigned code, len;
#endif

static void EncodeChar(unsigned c)
{
    unsigned i;
    int j, k;

    i = 0;
    j = 0;
    k = prnt[c + T];

    /* travel from leaf to root */
    do {
        i >>= 1;

        /* if node's address is odd-numbered, choose bigger brother node */
        if (k & 1) i += 0x8000;

        j++;
    } while ((k = prnt[k]) != R);
    Putcode(j, i);
#if 0
    code = i;
    len = j;
#endif
    update(c);
}

static void EncodePosition(unsigned c)
{
    unsigned i;

    /* output upper 6 bits by table lookup */
    i = c >> 6;
    Putcode(p_len[i], (unsigned)p_code[i] << 8);

    /* output lower 6 bits verbatim */
    Putcode(6, (c & 0x3f) << 10);
}

static void EncodeEnd(void)
{
    if (putlen) {
        if (putc(putbuf >> 8, outfile) == EOF) {
            Error(wterr);
        }
#ifdef VERBOSE
        codesize++;
#endif
    }
}
#endif

#ifdef DECODE
static int DecodeChar(void)
{
    unsigned c;

    c = son[R];

    /* travel from root to leaf, */
    /* choosing the smaller child node (son[]) if the read bit is 0, */
    /* the bigger (son[]+1} if 1 */
    while (c < T) {
        c += GetBit();
        c = son[c];
    }
    c -= T;
    update(c);
    return c;
}

static int DecodePosition(void)
{
    unsigned i, j, c;

    /* recover upper 6 bits from table */
    i = GetByte();
    c = (unsigned)d_code[i] << 6;
    j = d_len[i];

    /* read lower 6 bits verbatim */
    j -= 2;
    while (j--) {
        i = (i << 1) + GetBit();
    }
    return c | (i & 0x3f);
}
#endif

#ifdef ENCODE
/* compression */

void Encode(void)  /* compression */
{
    int  i, c, len, r, s, last_match_length;
    unsigned long tw;

    fseek(infile, 0L, 2);
    textsize = ftell(infile);
#ifdef VERBOSE
    if ((signed long)textsize < 0)
      Fprintf((stderr, "Errno: %d", errno));
#endif
    tw = host_to_i86ul(textsize);
    if (fwrite(&tw, sizeof tw, 1, outfile) < 1)
        Error(wterr);   /* output size of text */
    if (textsize == 0)
        return;
    rewind(infile);
    textsize = 0;           /* rewind and re-read */
    StartHuff();
    InitTree();
    s = 0;
    r = N - F;
    for (i = s; i < r; i++)
        text_buf[i] = ' ';
    for (len = 0; len < F && (c = getc(infile)) != EOF; len++)
        text_buf[r + len] = c;
    textsize = len;
    for (i = 1; i <= F; i++)
        InsertNode(r - i);
    InsertNode(r);
    do {
        if (match_length > len)
            match_length = len;
        if (match_length <= THRESHOLD) {
            match_length = 1;
            EncodeChar(text_buf[r]);
        } else {
            EncodeChar(255 - THRESHOLD + match_length);
            EncodePosition(match_position);
        }
        last_match_length = match_length;
        for (i = 0; i < last_match_length &&
                (c = getc(infile)) != EOF; i++) {
            DeleteNode(s);
            text_buf[s] = c;
            if (s < F - 1)
                text_buf[s + N] = c;
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            InsertNode(r);
        }
        if ((textsize += i) > printcount) {
#if defined(VERBOSE) && defined(EXTRAVERBOSE)
            Fprintf((stderr, "%12ld\r", textsize));
#endif
            printcount += 1024;
        }
        while (i++ < last_match_length) {
            DeleteNode(s);
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            if (--len) InsertNode(r);
        }
    } while (len > 0);
    EncodeEnd();
#ifdef	LONG_REPORT
    Fprintf((stderr, "input size    %ld bytes\n", codesize));
    Fprintf((stderr, "output size   %ld bytes\n", textsize));
    Fprintf((stderr, "input/output  %.3f\n", (double)codesize / textsize));
#else
    Fprintf((stderr, "input/output = %ld/%ld = %.3f\n", codesize, textsize,
		(double)codesize / textsize));
#endif
}
#endif

#ifdef DECODE
void Decode(void)  /* recover */
{
    int  i, j, k, r, c;
    unsigned long int  count;
    unsigned long tw;

    if (fread(&tw, sizeof tw, 1, infile) < 1)
        Error("Can't read");  /* read size of text */
    textsize = i86ul_to_host(tw);
    if (textsize == 0)
        return;
    StartHuff();
    for (i = 0; i < N - F; i++)
        text_buf[i] = ' ';
    r = N - F;
    for (count = 0; count < textsize; ) {
        c = DecodeChar();
        if (c < 256) {
            if (putc(c, outfile) == EOF) {
                Error(wterr);
            }
            text_buf[r++] = c;
            r &= (N - 1);
            count++;
        } else {
            i = (r - DecodePosition() - 1) & (N - 1);
            j = c - 255 + THRESHOLD;
            for (k = 0; k < j; k++) {
                c = text_buf[(i + k) & (N - 1)];
                if (putc(c, outfile) == EOF) {
                    Error(wterr);
                }
                text_buf[r++] = c;
                r &= (N - 1);
                count++;
            }
        }
        if (count > printcount) {
#if defined(VERBOSE) && defined(EXTRAVERBOSE)
            Fprintf((stderr, "%12ld\r", count));
#endif
            printcount += 1024;
        }
    }
    Fprintf((stderr, "%12ld\n", count));
}
#endif

#ifdef MAIN
int main(int argc, char *argv[])
{
    char  *s;
    FILE  *f;
    int    c;

    if (argc == 2) {
	outfile = stdout;
	if ((f = tmpfile()) == NULL) {
	    perror("tmpfile");
	    return EXIT_FAILURE;
	}
	while ((c = getchar()) != EOF)
	    fputc(c, f);
	rewind(infile = f);
    }
    else if (argc != 4) {
        Fprintf((stderr, "'lzhuf e file1 file2' encodes file1 into file2.\n"
                "'lzhuf d file2 file1' decodes file2 into file1.\n"));
        return EXIT_FAILURE;
    }
    if (argc == 4) {
	if ((s = argv[1], s[1] || strpbrk(s, "DEde") == NULL)
	  || (s = argv[2], (infile  = fopen(s, "rb")) == NULL)
	  || (s = argv[3], (outfile = fopen(s, "wb")) == NULL)) {
	    Fprintf((stderr, "??? %s\n", s));
	    return EXIT_FAILURE;
	}
    }
    if (toupper(*argv[1]) == 'E')
        Encode();
    else
        Decode();
    fclose(infile);
    fclose(outfile);
    return EXIT_SUCCESS;
}
#endif
