#include <etherboot.h>
#include <sys_info.h>
#include "context.h"
#define DEBUG_THIS DEBUG_SYS_INFO
#include <debug.h>

void collect_multiboot_info(struct sys_info *);

void collect_sys_info(struct sys_info *info)
{

    /* Pick up paramters given by bootloader to us */
    info->boot_type = boot_ctx->eax;
    info->boot_data = boot_ctx->ebx;
    info->boot_arg = boot_ctx->param[0];
    debug("boot eax = %#lx\n", info->boot_type);
    debug("boot ebx = %#lx\n", info->boot_data);
    debug("boot arg = %#lx\n", info->boot_arg);

    collect_elfboot_info(info);
    collect_linuxbios_info(info);
#ifdef MULTIBOOT_IMAGE
    collect_multiboot_info(info);
#endif

#if 0
    debug("RAM %Ld MB\n", (meminfo.memsize + 512*1024) >> 20);
#endif
}
