#ifndef ETHERBOOT_SETJMP_H
#define ETHERBOOT_SETJMP_H


/* Define a type for use by setjmp and longjmp */
#define JBLEN  70

typedef long jmp_buf[JBLEN] __attribute__ ((aligned (16))); /* guarantees 128-bit alignment! */

extern int setjmp (jmp_buf env);
extern void longjmp (jmp_buf env, int val);

#endif /* ETHERBOOT_SETJMP_H */
