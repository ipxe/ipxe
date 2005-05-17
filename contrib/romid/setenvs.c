/* subroutine to put a value string into an environment symbol.
   Uses the controling command.com environment, not the programs.
   This means that the env variable is set so other routines in
   a .BAT file may use it.

   call:  settheenv (char * symbol, char * val);
   symbol is an asciiz string containing the env variable name,
   val    is an asciiz string containing the value to assign to this vbl.

   returns: 0 = OK,
            1 = failure.
   failure is not unlikely.  The env block may be full.  Or on some
   systems the env block might not be found

   SETENVS.C was written by Richard Marks <rmarks@KSP.unisys.COM>.
*/


#include <stdio.h>
#include <dos.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char fill1[0x0A];
    int *prev_term_handler;
    int *prev_ctrl_c;
    int *prev_crit_error;
    char fill2[0x16];
    int  envir_seg;
} psp;

typedef struct {
    char  type;
    int   psp_segment;
    int   num_segments;
    char  fill[11];
    char  arena_data;
} arena;


#define NORMAL_ATYPE 0x4D
#define LAST_ATYPE   0x5A


static arena * get_next_arena (arena * ap) {
    return( MK_FP( FP_SEG(ap)+1+ap->num_segments, 0) );
}

/* returns 0 if passed pointer is to an arena, else returns 1 */
static int is_valid_arena (arena * ap) {
    arena * ap1;
    if (ap->type == NORMAL_ATYPE  &&
          (ap1=get_next_arena(ap))->type == NORMAL_ATYPE  &&
          ( (ap1=get_next_arena(ap1))->type == NORMAL_ATYPE  ||
          ap1->type == LAST_ATYPE) )
            return(0);
    return (1);
}


static arena * get_first_arena () {
/* return pointer to the first arena.
 * scan memory for a 0x4D on a segment start,
 * see if this points to another two levels of arena
 */
    arena * ap, * ap1;
    int * temp;
    int segment;

    for (segment=0; segment<_CS;  segment++) {
        ap = MK_FP(segment, 0);
        if ( is_valid_arena (ap) == 0)  return (ap);
    }
    return(NULL);
} /* end get_first_arena */


static int is_valid_env (char * ad, int num_segs) {
    char * base_ad;
    base_ad = ad;
    while ( (*ad) && (((ad-base_ad)>>4) < num_segs) ) {
        if (strnicmp(ad, "COMSPEC=", 8)==0)  return(0);
        ad += strlen(ad) + 1;
    }
    return (1);
}


static arena * get_arena_of_environment () {
/* to get the arena of first environment block:
   First get segment of COMMAND.COM from segment of previous critical err code.
   Then scan all the arenas for an environment block with a matching PSP
   segment */

arena * ap;
psp   * pspp, * pspc;
unsigned int i, ccseg;

/* set pspp to psp of this program */
pspp = MK_FP(_psp,0);

#ifdef DEBUG
printf("prog psp=%p\n",pspp);
#endif

/* set pspc to psp of COMMAND.COM, back up a bit to get it if needed */
ccseg = FP_SEG (pspp->prev_crit_error);
if ( (i=ccseg-32) < 60)  i=60;

while (ccseg>i) {
    pspc = MK_FP (ccseg, 0);
    if ( is_valid_arena((arena *) pspc) == 0)  goto L1;
    ccseg--;
}
return (NULL);

L1: pspc = MK_FP (++ccseg, 0);
#ifdef DEBUG
printf("comm.com=%p\n",pspc);
#endif

/* first see if env seg in command.com points to valid env block
   if env seg is in a valid arena, then arena must point to this command.com
   else assume env block is fabricated like for 4DOS, use 128 bytes */

ap = MK_FP (pspc->envir_seg-1, 0);
i  = ap->num_segments;

if (is_valid_arena (ap) == 0) {
    if (ap->psp_segment != FP_SEG(pspc))  goto L2;
} else {
    i = 9;
}

if ( is_valid_env (&ap->arena_data, i) == 0 )
    return (ap);

/* command.com did not so point, search thru all env blocks */

L2:
if ( (ap=get_first_arena()) != NULL ) {
    while (ap->type != LAST_ATYPE) {
#ifdef DEBUG
        printf("%p\n",ap);
#endif
        if (ap->psp_segment == FP_SEG(pspc) &&
            is_valid_env (&ap->arena_data, ap->num_segments)==0 )
            return (ap);

        ap = get_next_arena(ap);
    }
} return(NULL);
}  /* end get_arena_of_environment */

/*****************************************************************************/

int settheenv(char * symbol, char * val) {
int total_size,
    needed_size=0,
    strlength;
char * sp, *op, *envir;
char symb_len=strlen(symbol);
char found=0;
arena * ap;

strupr(symbol);

/* first, can COMMAND.COM's envir block be found ? */
if ( (ap=get_arena_of_environment()) == NULL)
    return(1);

/* search to end of the envir block, get sizes */
total_size = 16 * ap->num_segments;
envir = &ap->arena_data;
op=sp=envir;
while (*sp) {
    strlength = strlen(sp)+1;
    if ( *(sp+symb_len)=='='  &&
         strnicmp(sp,symbol,symb_len)==0 )
        found=1;
    else {
        needed_size += strlength;
        if (found) strcpy(op,sp);
        op = &op[strlength];
    }
    sp += strlength;
}
*op=0;
if (strlen(val) > 0) {
    needed_size += 3 + strlen(symbol) + strlen(val);
    if (needed_size > total_size)
        return(1);  /* could mess with environment expansion here */

    strcpy(op, symbol); strcat(op, "="); strcat(op, val);
    op += strlen(op)+1;
    *op = 0;
}
return(0);
} /* end setheenv subroutine */
