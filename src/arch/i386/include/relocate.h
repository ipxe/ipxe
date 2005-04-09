#ifndef RELOCATE_H
#define RELOCATE_H

#ifdef KEEP_IT_REAL

/* relocate() is conceptually impossible with KEEP_IT_REAL */
#define relocate()

#else

extern void relocate ( void );

#endif

#endif /* RELOCATE_H */
