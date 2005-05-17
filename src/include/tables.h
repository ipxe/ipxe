#ifndef TABLES_H
#define TABLES_H

/*
 * Macros for dealing with linker-generated tables of fixed-size
 * symbols.  We make fairly extensive use of these in order to avoid
 * ifdef spaghetti and/or linker symbol pollution.  For example,
 * instead of having code such as
 *
 * #ifdef CONSOLE_SERIAL
 *   serial_init();
 * #endif
 *
 * we make serial.c generate an entry in the initialisation function
 * table, and then have a function call_init_fns() that simply calls
 * all functions present in this table.  If and only if serial.o gets
 * linked in, then its initialisation function will be called.  We
 * avoid linker symbol pollution (i.e. always dragging in serial.o
 * just because of a call to serial_init()) and we also avoid ifdef
 * spaghetti (having to conditionalise every reference to functions in
 * serial.c).
 *
 * The linker script takes care of assembling the tables for us.  All
 * our table sections have names of the format ".tbl.NAME.NN" where
 * NAME designates the data structure stored in the table
 * (e.g. "init_fn") and NN is a two-digit decimal number used to
 * impose an ordering upon the tables if required.  NN=00 is reserved
 * for the symbol indicating "table start", and NN=99 is reserved for
 * the symbol indicating "table end".
 *
 * To define an entry in the "xxx" table:
 *
 *  static struct xxx my_xxx __table(xxx,01) = { ... };
 *
 * To access start and end markers for the "xxx" table:
 *
 *  static struct xxx xxx_start[0] __table_start(xxx);
 *  static struct xxx xxx_end[0] __table_end(xxx);
 *
 * See init.h and init.c for an example of how these macros are used
 * in practice.
 *
 */

#define __table_str(x) #x
#define __table_section(table,idx) \
	__section__ ( ".tbl." __table_str(table) "." __table_str(idx) )

#define __table_section_start(table) __table_section(table,00)
#define __table_section_end(table) __table_section(table,99)

#define __table(table,idx) \
	__attribute__ (( unused, __table_section(table,idx) ))
#define __table_start(table) \
	__attribute__ (( unused, __table_section_start(table) ))
#define __table_end(table) \
	__attribute__ (( unused, __table_section_end(table) ))

#endif /* TABLES_H */
