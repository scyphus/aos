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

#define MIN_YEAR        2013
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
    /* Read status register A */
    outb(CMOS_ADDR, 0x0a);
    return inb(CMOS_DATA) & 0x80;
}

static u8
_get_rtc_reg(u8 reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

/*
 *
 */
static u64
_datetime_to_unixtime(u32 year, u8 month, u8 day, u8 hour, u8 min, u8 sec)
{
    u64 unixtime;
    u64 days;

    unixtime = 0;

    /* Year */
    days = 365 * (year - 1970);

    /* Leap year */
    days += ((year - 1) / 4) - 1969 / 4;
    days -= ((year - 1) / 100) - 1969 / 100;
    days += ((year - 1) / 400) - 1969 / 400;

    /* Month */
    switch ( month ) {
    case 12:
        days += 30;
    case 11:
        days += 31;
    case 10:
        days += 30;
    case 9:
        days += 31;
    case 8:
        days += 31;
    case 7:
        days += 30;
    case 6:
        days += 31;
    case 5:
        days += 30;
    case 4:
        days += 31;
    case 3:
        days += 28;
        if ( (0 == (year % 4) && 0 != (year % 100)) || 0 == (year % 400) ) {
            days += 1;
        }
    case 2:
        days += 31;
    case 1:
        ;
    }

    /* Day */
    days += day - 1;

    unixtime = (((days * 24 + hour) * 60 + min)) * 60 + sec;

    return unixtime;
}

/*
 * Read date & time from CMOS RTC in Unix timestamp
 */
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
            century = _get_rtc_reg(century_reg);
        }
    } while ( last_sec != sec || last_min != min || last_hour != hour
              || last_day != day || last_month != month || last_year != year
              || last_century != century );

    /* Read status register B */
    regb = _get_rtc_reg(0x0b);

    if ( !(regb & 0x04) ) {
        /* Convert BCD to binary values if necessary */
        sec = (sec & 0x0f) + (sec >> 4) * 10;
        min = (min & 0x0f) + (min >> 4) * 10;
        day = (day & 0x0f) + (day >> 4) * 10;
        month = (month & 0x0f) + (month >> 4) * 10;
        year = (year & 0x0f) + (year >> 4) * 10;
        if ( century_reg ) {
            century = (century & 0x0f) + (century >> 4) * 10;
        }

        if ( regb & 0x02 ) {
            /* 12-hour format to 24-hour format */
            if ( hour & 0x80 ) {
                /* PM */
                hour = hour & 0x7f;
                hour = (hour & 0x0f) + (hour >> 4) * 10;
                hour = (hour + 12) % 24;
            } else {
                hour = (hour & 0x0f) + (hour >> 4) * 10;
            }
        } else {
            hour = (hour & 0x0f) + (hour >> 4) * 10;
        }
    } else {
        if ( regb & 0x02 ) {
            /* 12-hour format to 24-hour format */
            if ( hour & 0x80 ) {
                /* PM */
                hour = ((hour & 0x7f) + 12) % 24;
            }
        }
    }

    if ( year >= MIN_YEAR % 100 ) {
        century = MIN_YEAR / 100;
    } else {
        century = MIN_YEAR / 100 + 1;
    }

    return _datetime_to_unixtime((u32)century * 100 + (u32)year,
                                 month, day, hour, min, sec);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
