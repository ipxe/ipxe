#ifndef IA64_PAL_H
#define IA64_PAL_H

struct pal_freq_ratio {
	unsigned long den : 32, num : 32;	/* numerator & denominator */
};
extern long pal_freq_ratios(struct pal_freq_ratio *proc_ratio, 
	struct pal_freq_ratio *bus_ratio, struct pal_freq_ratio *itc_ratio);


#endif /* IA64_PAL_H */
