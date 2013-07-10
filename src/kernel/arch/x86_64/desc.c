/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "desc.h"

#define CODE_SEL_RING0  0x8
#define IDT_PRESENT     0x80
#define IDT_INTGATE     0x0e
struct idtr idtr;
struct idt_gate_desc idt[256];
void intr_null(void);
void intr_gp(void);

static void
idt_setup_gate_desc(int nr, u64 base, u16 selector, u8 flags)
{
    struct idt_gate_desc *idt;

    idt = (struct idt_gate_desc *)
        (idtr.base + nr * sizeof(struct idt_gate_desc));

    idt->target_lo = (u16)(base & 0xffff);
    idt->selector = (u16)selector;
    idt->reserved1 = 0;
    idt->flags = flags;// + 0x60;
    idt->target_mid = (u16)((base & 0xffff0000UL) >> 16);
    idt->target_hi = (u16)((base & 0xffffffff00000000UL) >> 32);
    idt->reserved2 = 0;
}
void
idt_setup_intr_gate(int nr, void *target)
{
    idt_setup_gate_desc(nr, (u64)target, CODE_SEL_RING0,
                        IDT_PRESENT | IDT_INTGATE);
}

void
idt_init(void)
{
    int i;
    u64 sz = 256 * sizeof(struct idt_gate_desc);

    //(u64)kmem_alloc_pages((sz - 1) / PAGESIZE + 1)
    idtr.base = (u64)idt;
    idtr.size = sz - 1;

    for ( i = 0; i < 256; i++ ) {
        idt_setup_intr_gate(i, &intr_null);
    }

    void *x = &idtr;
    //__asm__ __volatile__ ( "lidt _idtr" );
    __asm__ __volatile__ ( "lidt (%0)" : : "p"(x) );
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
