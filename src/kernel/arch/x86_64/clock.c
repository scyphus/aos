/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "arch.h"
#include "clock.h"
#include "acpi.h"

static u64 clock_acpi_cnt;
static u32 clock_acpi_prev;
static volatile int lock;

/*
 * Initialize clock
 */
void
clock_init(void)
{
    lock = 0;
    clock_acpi_cnt = 0;
    clock_acpi_prev = acpi_get_timer();
}

/*
 * Get current clock
 */
u64
clock_get(void)
{
    return 1000000000ULL * clock_acpi_cnt / acpi_get_timer_hz();
}

/*
 * Update clock
 */
void
clock_update(void)
{
    u32 cur;

    spin_lock(&lock);

    cur = acpi_get_timer();
    if ( cur < clock_acpi_prev ) {
        clock_acpi_cnt += acpi_get_timer_period() - clock_acpi_prev + cur;
    } else {
        clock_acpi_cnt += (u64)cur - clock_acpi_prev;
    }
    clock_acpi_prev = cur;

    spin_unlock(&lock);
}


void
callout_queue(void)
{

}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
