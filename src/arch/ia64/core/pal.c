#include "etherboot.h"
#include "sal.h"
#include "pal.h"

struct fptr pal_entry;
/*
 * Note that some of these calls use a static-register only calling
 * convention which has nothing to do with the regular calling
 * convention.
 */
#define PAL_CACHE_FLUSH		1	/* flush i/d cache */
#define PAL_CACHE_INFO		2	/* get detailed i/d cache info */
#define PAL_CACHE_INIT		3	/* initialize i/d cache */
#define PAL_CACHE_SUMMARY	4	/* get summary of cache heirarchy */
#define PAL_MEM_ATTRIB		5	/* list supported memory attributes */
#define PAL_PTCE_INFO		6	/* purge TLB info */
#define PAL_VM_INFO		7	/* return supported virtual memory features */
#define PAL_VM_SUMMARY		8	/* return summary on supported vm features */
#define PAL_BUS_GET_FEATURES	9	/* return processor bus interface features settings */
#define PAL_BUS_SET_FEATURES	10	/* set processor bus features */
#define PAL_DEBUG_INFO		11	/* get number of debug registers */
#define PAL_FIXED_ADDR		12	/* get fixed component of processors's directed address */
#define PAL_FREQ_BASE		13	/* base frequency of the platform */
#define PAL_FREQ_RATIOS		14	/* ratio of processor, bus and ITC frequency */
#define PAL_PERF_MON_INFO	15	/* return performance monitor info */
#define PAL_PLATFORM_ADDR	16	/* set processor interrupt block and IO port space addr */
#define PAL_PROC_GET_FEATURES	17	/* get configurable processor features & settings */
#define PAL_PROC_SET_FEATURES	18	/* enable/disable configurable processor features */
#define PAL_RSE_INFO		19	/* return rse information */
#define PAL_VERSION		20	/* return version of PAL code */
#define PAL_MC_CLEAR_LOG	21	/* clear all processor log info */
#define PAL_MC_DRAIN		22	/* drain operations which could result in an MCA */
#define PAL_MC_EXPECTED		23	/* set/reset expected MCA indicator */
#define PAL_MC_DYNAMIC_STATE	24	/* get processor dynamic state */
#define PAL_MC_ERROR_INFO	25	/* get processor MCA info and static state */
#define PAL_MC_RESUME		26	/* Return to interrupted process */
#define PAL_MC_REGISTER_MEM	27	/* Register memory for PAL to use during MCAs and inits */
#define PAL_HALT		28	/* enter the low power HALT state */
#define PAL_HALT_LIGHT		29	/* enter the low power light halt state*/
#define PAL_COPY_INFO		30	/* returns info needed to relocate PAL */
#define PAL_CACHE_LINE_INIT	31	/* init tags & data of cache line */
#define PAL_PMI_ENTRYPOINT	32	/* register PMI memory entry points with the processor */
#define PAL_ENTER_IA_32_ENV	33	/* enter IA-32 system environment */
#define PAL_VM_PAGE_SIZE	34	/* return vm TC and page walker page sizes */

#define PAL_MEM_FOR_TEST	37	/* get amount of memory needed for late processor test */
#define PAL_CACHE_PROT_INFO	38	/* get i/d cache protection info */
#define PAL_REGISTER_INFO	39	/* return AR and CR register information*/
#define PAL_SHUTDOWN		40	/* enter processor shutdown state */
#define PAL_PREFETCH_VISIBILITY	41

#define PAL_COPY_PAL		256	/* relocate PAL procedures and PAL PMI */
#define PAL_HALT_INFO		257	/* return the low power capabilities of processor */
#define PAL_TEST_PROC		258	/* perform late processor self-test */
#define PAL_CACHE_READ		259	/* read tag & data of cacheline for diagnostic testing */
#define PAL_CACHE_WRITE		260	/* write tag & data of cacheline for diagnostic testing */
#define PAL_VM_TR_READ		261	/* read contents of translation register */


/*
 * Get the ratios for processor frequency, bus frequency and interval timer to
 * to base frequency of the platform
 */
long pal_freq_ratios(struct pal_freq_ratio *proc_ratio, 
	struct pal_freq_ratio *bus_ratio, struct pal_freq_ratio *itc_ratio)
{
	struct freq_ratios {
		long status;
		struct pal_freq_ratio proc_ratio;
		struct pal_freq_ratio bus_ratio;
		struct pal_freq_ratio itc_ratio;
	};
	struct freq_ratios result;
	extern struct freq_ratios pal_call(unsigned long which, ...);
	result = pal_call(PAL_FREQ_RATIOS, 0, 0, 0);
	if (proc_ratio)
		*proc_ratio = result.proc_ratio;
	if (bus_ratio)
		*bus_ratio = result.bus_ratio;
	if (itc_ratio)
		*itc_ratio = result.itc_ratio;
	return result.status;
	
}
