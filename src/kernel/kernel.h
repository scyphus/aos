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


int kprintf(const char *, ...);
void panic(const char *);

/* Architecture-dependent functions */
void arch_bsp_init(void);
void arch_ap_init(void);

void arch_putc(int);
void arch_busy_usleep(u64);
void arch_halt(void);

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
