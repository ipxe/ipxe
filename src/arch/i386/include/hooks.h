#ifndef HOOKS_H
#define HOOKS_H

/* in hooks.o */
extern void arch_initialise ( struct i386_all_regs *regs,
			      void (*retaddr) (void) );
extern void arch_main ( struct i386_all_regs *regs );

/* in hooks_rm.o */
extern void arch_rm_initialise ( struct i386_all_regs *regs,
				 void (*retaddr) (void) );
extern void arch_rm_main ( struct i386_all_regs *regs );

#endif /* HOOKS_H */
