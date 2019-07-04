/*
 * Coverity modelling file
 *
 */

typedef long off_t;
typedef void * userptr_t;
typedef long long time_t;
struct tm;

/* Inhibit use of built-in models for functions where Coverity's
 * assumptions about the modelled function are incorrect for iPXE.
 */
char * strerror ( int errno ) {
}
void copy_from_user ( void *dest, userptr_t src, off_t src_off, size_t len ) {
}
time_t mktime ( struct tm *tm ) {
}
int getchar ( void ) {
}
