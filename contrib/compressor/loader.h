/* Do not change these values unless you really know what you are doing;
   the pre-computed lookup tables rely on the buffer size being 4kB or
   smaller. The buffer size must be a power of two. The lookahead size has
   to fit into 6 bits. If you change any of these numbers, you will also
   have to adjust the decompressor accordingly.
 */

#define BUFSZ           4096
#define LOOKAHEAD       60
#define THRESHOLD       2
#define NCHAR           (256+LOOKAHEAD-THRESHOLD)
#define TABLESZ         (NCHAR+NCHAR-1)
#define NIL             ((unsigned short)-1)

