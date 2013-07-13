/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_DESC_H
#define _KERNEL_DESC_H

#include <aos/const.h>



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

struct gdt_desc {
    u16 w0;
    u16 w1;
    u16 w2;
    u16 w3;
} __attribute__ ((packed));

struct gdt_desc_tss {
    u16 w0;
    u16 w1;
    u16 w2;
    u16 w3;
    u16 w4;
    u16 w5;
    u16 w6;
    u16 w7;
} __attribute__ ((packed));

struct gdtr {
    u16 size;
    u64 base;
} __attribute__ ((packed));


void idt_setup_intr_gate(int, void *);
void idt_init(void);
void idt_load(void);

void gdt_init(void);
void gdt_load(void);

void tss_init(void);
void tr_load(int);

#endif /* _KERNEL_DESC_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
