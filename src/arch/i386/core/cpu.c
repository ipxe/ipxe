#ifdef CONFIG_X86_64
#include "stdint.h"
#include "string.h"
#include "bits/cpu.h"


/* Standard macro to see if a specific flag is changeable */
static inline int flag_is_changeable_p(uint32_t flag)
{
	uint32_t f1, f2;

	asm("pushfl\n\t"
	    "pushfl\n\t"
	    "popl %0\n\t"
	    "movl %0,%1\n\t"
	    "xorl %2,%0\n\t"
	    "pushl %0\n\t"
	    "popfl\n\t"
	    "pushfl\n\t"
	    "popl %0\n\t"
	    "popfl\n\t"
	    : "=&r" (f1), "=&r" (f2)
	    : "ir" (flag));

	return ((f1^f2) & flag) != 0;
}


/* Probe for the CPUID instruction */
static inline int have_cpuid_p(void)
{
	return flag_is_changeable_p(X86_EFLAGS_ID);
}

static void identify_cpu(struct cpuinfo_x86 *c)
{
	unsigned xlvl;

	c->cpuid_level = -1;		/* CPUID not detected */
	c->x86_model = c->x86_mask = 0;	/* So far unknown... */
	c->x86_vendor_id[0] = '\0';	/* Unset */
	memset(&c->x86_capability, 0, sizeof c->x86_capability);
	
	if (!have_cpuid_p()) {
		/* CPU doesn'thave CPUID */

		/* If there are any capabilities, they'r vendor-specific */
		/* enable_cpuid() would have set c->x86 for us. */
	}
	else {
		/* CPU does have CPUID */

		/* Get vendor name */
		cpuid(0x00000000, &c->cpuid_level,
		      (int *)&c->x86_vendor_id[0],
		      (int *)&c->x86_vendor_id[8],
		      (int *)&c->x86_vendor_id[4]);
		
		/* Initialize the standard set of capabilities */
		/* Note that the vendor-specific code below might override */

		/* Intel-defined flags: level 0x00000001 */
		if ( c->cpuid_level >= 0x00000001 ) {
			unsigned tfms, junk;
			cpuid(0x00000001, &tfms, &junk, &junk,
			      &c->x86_capability[0]);
			c->x86 = (tfms >> 8) & 15;
			c->x86_model = (tfms >> 4) & 15;
			c->x86_mask = tfms & 15;
		}

		/* AMD-defined flags: level 0x80000001 */
		xlvl = cpuid_eax(0x80000000);
		if ( (xlvl & 0xffff0000) == 0x80000000 ) {
			if ( xlvl >= 0x80000001 )
				c->x86_capability[1] = cpuid_edx(0x80000001);
		}
	}
}

struct cpuinfo_x86 cpu_info;
void cpu_setup(void)
{
	identify_cpu(&cpu_info);
}
#endif /* CONFIG_X86_64 */
