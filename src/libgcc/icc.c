/*
 * Intel's compiler creates an implicit call to this function at the
 * start of main().
 *
 */
void __attribute__ (( cdecl )) __intel_new_proc_init ( void ) {
	/* Do nothing */
}
