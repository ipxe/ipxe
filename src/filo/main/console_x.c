
#include "etherboot.h"

#include <lib.h>

int getline(char *buf, int max)
{               
    int cur, ch, nonspace_seen;
            
    cur = 0;
    while (buf[cur]) {
        putchar(buf[cur]);
        cur++;  
    }
    for (;;) {
        ch = getchar();
        switch (ch) {
        /* end of line */
        case '\r':
        case '\n':
            putchar('\n');
            goto out;
        /* backspace */
        case '\b':
        case '\x7f':
            if (cur > 0) {
                cur--;
                putchar('\b');
                putchar(' ');
                putchar('\b');
            }
            break;
        /* word erase */
        case 'W' & 0x1f: /* ^W */
            nonspace_seen = 0;
            while (cur) {
                if (buf[cur-1] != ' ')
                    nonspace_seen = 1;
                putchar('\b');
                putchar(' ');
                putchar('\b');
                cur--;
                if (nonspace_seen && cur < max-1 && cur > 0 && buf[cur-1]==' ')
                    break;
            }
            break;
        /* line erase */
        case 'U' & 0x1f: /* ^U */
            while (cur) {
                putchar('\b');
                putchar(' ');
                putchar('\b');
                cur--;
            }
            cur = 0;
            break;
        default:
            if (ch < 0x20)
                break; /* ignore control char */
            if (ch >= 0x7f)
                break;
            if (cur + 1 < max) {
                putchar(ch); /* echo back */
                buf[cur] = ch;
                cur++;
            }
        }
    }
out:
    if (cur >= max)
        cur = max - 1;
    buf[cur] = '\0';
    return cur;
}                                                   
