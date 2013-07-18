/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "cmos.h"
#include "arch.h"

#define DEFAULT_CENTURY 20
#define CMOS_ADDR       0x70
#define CMOS_DATA       0x71

static int _is_updating(void);
static u8 _get_rtc_reg(u8);

/*
 * Check if the CMOS is updating (in progress)
 */
static int
_is_updating(void)
{
    /* Register A */
    outb(CMOS_ADDR, 0x0a);
    return inb(CMOS_DATA) & 0x80;
}

static u8
_get_rtc_reg(u8 reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

u64
cmos_rtc_read_datetime(u8 century_reg)
{
    u8 sec;
    u8 min;
    u8 hour;
    u8 day;
    u8 month;
    u8 year;
    u8 century;
    u8 last_sec;
    u8 last_min;
    u8 last_hour;
    u8 last_day;
    u8 last_month;
    u8 last_year;
    u8 last_century;
    u8 regb;

    /* Wait until the CMOS is ready for use */
    while ( _is_updating() );

    sec = _get_rtc_reg(0x00);
    min = _get_rtc_reg(0x02);
    hour = _get_rtc_reg(0x04);
    day = _get_rtc_reg(0x07);
    month = _get_rtc_reg(0x08);
    year = _get_rtc_reg(0x09);
    if ( century_reg ) {
        century = _get_rtc_reg(0x09);
    } else {
        century = DEFAULT_CENTURY;
    }

    do {
        last_sec = sec;
        last_min = min;
        last_hour = hour;
        last_day = day;
        last_month = month;
        last_year = year;
        last_century = century;

        while ( _is_updating() );
        sec = _get_rtc_reg(0x00);
        min = _get_rtc_reg(0x02);
        hour = _get_rtc_reg(0x04);
        day = _get_rtc_reg(0x07);
        month = _get_rtc_reg(0x08);
        year = _get_rtc_reg(0x09);
        if ( century_reg ) {
            century = _get_rtc_reg(0x09);
        }
    } while ( last_sec != sec || last_min != min || last_hour != hour
              || last_day != day || last_month != month || last_year != year
              || last_century != century );

    regb = _get_rtc_reg(0x0b);

    if ( !(regb & 0x04) ) {
        /* Convert BCD to binary values if necessary */
        sec = (sec & 0x0f) + (sec >> 4) * 10;
        min = (min & 0x0f) + (min >> 4) * 10;
        hour = (hour & 0x0f) + (hour >> 4) * 10;
        day = (day & 0x0f) + (day >> 4) * 10;
        month = (month & 0x0f) + (month >> 4) * 10;
        century = (century & 0x0f) + (century >> 4) * 10;

        if ( regb & 0x02 ) {
            /* 12-hour format to 24-hour format */
            if ( year & 0x80 ) {
                /* PM */
                year = year & 0x7f;
                year = (year & 0x0f) + (year >> 4) * 10;
                year = (year + 12) % 24;
            }
        }
    } else {
        if ( regb & 0x02 ) {
            /* 12-hour format to 24-hour format */
            if ( year & 0x80 ) {
                /* PM */
                year = ((year & 0x7f) + 12) % 24;
            }
        }
    }

    kprintf("Time: %02d%02d-%02d-%02d %02d:%02d:%02d\r\n",
            century, year, month, day, hour, min, sec);

    return 0;
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
