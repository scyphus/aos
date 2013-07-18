/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#ifndef _KERNEL_H
#define _KERNEL_H

#include <aos/const.h>


#define IV_TMR          0x20
#define IV_KBD          0x21
#define IV_LOC_TMR      0x50


int kprintf(const char *, ...);
void panic(const char *);

/* Architecture-dependent functions */
void arch_bsp_init(void);
void arch_ap_init(void);

void arch_putc(int);
void arch_busy_usleep(u64);
void arch_crash(void);

void arch_spin_lock(int *);
void arch_spin_unlock(int *);

#endif /* _KERNEL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
