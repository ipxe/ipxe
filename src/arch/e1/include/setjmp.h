#ifndef _SETJMP_H
#define _SETJMP_H


typedef struct {
	unsigned long G3;
	unsigned long G4;
	unsigned long SavedSP;
	unsigned long SavedPC;
	unsigned long SavedSR;
	unsigned long ReturnValue;
} __jmp_buf[1];

typedef struct __jmp_buf_tag	/* C++ doesn't like tagless structs.  */
  {
    __jmp_buf __jmpbuf;		/* Calling environment.  */
    int __mask_was_saved;	/* Saved the signal mask?  */
  } jmp_buf[1];

void longjmp(jmp_buf state, int value );
int setjmp( jmp_buf state);

#endif
