/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_H
#define _KERNEL_H

/*
 * IDT
 */
struct idt_gate_desc {
    u16 target_lo;
    u16 selector;
    u8 reserved1;
    u8 flags;
    u16 target_mid;
    u32 target_hi;
    u32 reserved2;
} __attribute__ ((packed));
struct idtr {
    u16 size;
    u64 base;   /* (idt_gate_descriptor *) */
} __attribute__ ((packed));


void idt_setup_intr_gate(int, void *);
void idt_init(void);


void intr_gpf(void);
u64 rdtsc(void);
u64 rdmsr(u64);
void pause(void);
int is_invariant_tsc(void);
int get_cpu_family(void);
int get_cpu_model(void);

int acpi_load_rsdp(void);


extern u64 ioapic_base;
extern u64 pm_tmr_port;

#endif /* _KERNEL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
