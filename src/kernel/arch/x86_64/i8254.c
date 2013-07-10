/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "arch.h"
#include "i8254.h"

#define I8254_REG_CNTR0         0x0040
#define I8254_REG_CTRL          0x0043
#define I8254_HZ                1193182
#define I8254_CTRL_SQUAREWAVE   0x06
/* 16bit read/load */
#define I8254_CTRL_RL_16BIT     0x30

/*
 * Start i8254 timer
 */
void
i8254_start_timer(int hz)
{
    int counter;

    counter = I8254_HZ / hz;
    outb(I8254_REG_CTRL, I8254_CTRL_RL_16BIT | I8254_CTRL_SQUAREWAVE);
    outb(I8254_REG_CNTR0, counter & 0xff);
    outb(I8254_REG_CNTR0, counter >> 8);
}

/*
 * Stop i8254 timer
 */
void
i8254_stop_timer(void)
{
    /* Reset to the BIOS default */
    outb(I8254_REG_CTRL, 0x36);
    outb(I8254_REG_CNTR0, 0x0);
    outb(I8254_REG_CNTR0, 0x0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
