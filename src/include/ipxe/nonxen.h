#ifndef _IPXE_NONXEN_H
#define _IPXE_NONXEN_H

/** @file
 *
 * Stub Xen definitions for platforms with no Xen support
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#define __XEN_GUEST_HANDLE(name)        __guest_handle_ ## name

#define XEN_GUEST_HANDLE(name)          __XEN_GUEST_HANDLE(name)

#define ___DEFINE_XEN_GUEST_HANDLE(name, type)	\
	typedef type * __XEN_GUEST_HANDLE(name)

#define __DEFINE_XEN_GUEST_HANDLE(name, type)			\
	___DEFINE_XEN_GUEST_HANDLE(name, type);			\
	___DEFINE_XEN_GUEST_HANDLE(const_##name, const type)

#define DEFINE_XEN_GUEST_HANDLE(name)   __DEFINE_XEN_GUEST_HANDLE(name, name)

typedef unsigned long xen_pfn_t;

typedef unsigned long xen_ulong_t;

struct arch_vcpu_info {};

struct arch_shared_info {};

#define XEN_LEGACY_MAX_VCPUS 0

struct xen_hypervisor;

static inline __attribute__ (( always_inline )) unsigned long
xen_hypercall_1 ( struct xen_hypervisor *xen __unused,
		  unsigned int hypercall __unused,
		  unsigned long arg1 __unused ) {
	return 1;
}

static inline __attribute__ (( always_inline )) unsigned long
xen_hypercall_2 ( struct xen_hypervisor *xen __unused,
		  unsigned int hypercall __unused,
		  unsigned long arg1 __unused, unsigned long arg2 __unused ) {
	return 1;
}

static inline __attribute__ (( always_inline )) unsigned long
xen_hypercall_3 ( struct xen_hypervisor *xen __unused,
		  unsigned int hypercall __unused,
		  unsigned long arg1 __unused, unsigned long arg2 __unused,
		  unsigned long arg3 __unused ) {
	return 1;
}

static inline __attribute__ (( always_inline )) unsigned long
xen_hypercall_4 ( struct xen_hypervisor *xen __unused,
		  unsigned int hypercall __unused,
		  unsigned long arg1 __unused, unsigned long arg2 __unused,
		  unsigned long arg3 __unused, unsigned long arg4 __unused ) {
	return 1;
}

static inline __attribute__ (( always_inline )) unsigned long
xen_hypercall_5 ( struct xen_hypervisor *xen __unused,
		  unsigned int hypercall __unused,
		  unsigned long arg1 __unused, unsigned long arg2 __unused,
		  unsigned long arg3 __unused, unsigned long arg4 __unused,
		  unsigned long arg5 __unused ) {
	return 1;
}

#endif /* _IPXE_NONXEN_H */
