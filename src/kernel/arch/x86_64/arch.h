/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_ARCH_H
#define _KERNEL_ARCH_H

#include <aos/const.h>

void intr_gpf(void);

u8 inb(u16);
u16 inw(u16);
u32 inl(u16);
void outb(u16, u8);
void outw(u16, u16);
void outl(u16, u32);
void sfence(void);
void lfence(void);
void mfence(void);

u64 rdtsc(void);
u64 rdmsr(u64);
void pause(void);
int is_invariant_tsc(void);
int get_cpu_family(void);
int get_cpu_model(void);

int acpi_load_rsdp(void);

void asm_ioapic_map_intr(u64, u64, u64);

void arch_busy_usleep(u64);

extern u64 acpi_ioapic_base;
extern u64 acpi_pm_tmr_port;


#endif /* _KERNEL_ARCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
