/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "acpi.h"
#include "arch.h"

u64 acpi_ioapic_base;
u64 acpi_pm_tmr_port;

/*
 * Compute checksum
 */
int
acpi_checksum(u8 *ptr, unsigned int len)
{
    u8 sum = 0;
    unsigned int i;

    for ( i = 0; i < len; i++ ) {
        sum += ptr[i];
    }

    return sum;
}

/*
 * Memcmp
 */
int
cmp(const u8 *a, const u8 *b, int len)
{
    int i;

    for ( i = 0; i < len; i++ ) {
        if ( a[i] != b[i] ) {
            return a[i] - b[i];
        }
    }

    return 0;
}


/*
 * APIC
 *   0: Processor Local APIC
 *   1: I/O APIC
 *   2: Interrupt Source Override
 */
int
acpi_parse_apic(struct acpi_sdt_hdr *sdt)
{
    u64 addr;
    struct acpi_sdt_apic *apic;
    struct acpi_sdt_apic_hdr *hdr;
    struct acpi_sdt_apic_lapic *lapic;
    struct acpi_sdt_apic_ioapic *ioapic;
    u32 len;

    len = 0;
    addr = (u64)sdt;
    len += sizeof(struct acpi_sdt_hdr);
    apic = (struct acpi_sdt_apic *)(addr + len);
    len += sizeof(struct acpi_sdt_apic);
    (void)apic;

    while ( len < sdt->length ) {
        hdr = (struct acpi_sdt_apic_hdr *)(addr + len);
        if ( len + hdr->length > sdt->length ) {
            /* Invalid */
            return -1;
        }
        switch  ( hdr->type ) {
        case 0:
            /* Local APIC */
            lapic = (struct acpi_sdt_apic_lapic *)hdr;
            (void)lapic;
            break;
        case 1:
            /* I/O APIC */
            ioapic = (struct acpi_sdt_apic_ioapic *)hdr;
            acpi_ioapic_base = ioapic->addr;
            break;
        case 2:
            /* Interrupt Source Override */
            break;
        default:
            /* Other */
            ;
        }
        len += hdr->length;
    }

    return 0;
}

int
acpi_parse_fadt(struct acpi_sdt_hdr *sdt)
{
    u64 addr;
    struct acpi_sdt_fadt *fadt;
    u32 len;

    len = 0;
    addr = (u64)sdt;
    len += sizeof(struct acpi_sdt_hdr);
    fadt = (struct acpi_sdt_fadt *)(addr + len);

    if ( sdt->revision >= 3 ) {
        /* FADT revision 2.0 or higher */
        if ( fadt->x_pm_timer_block.addr_space != 1 ) {
            /* Must be 1 (System I/O) */
            return -1;
        }
        acpi_pm_tmr_port = fadt->x_pm_timer_block.addr;
        if ( !acpi_pm_tmr_port ) {
            acpi_pm_tmr_port = fadt->pm_timer_block;
        }
    } else {
        /* Revision  */
        acpi_pm_tmr_port = fadt->pm_timer_block;
    }

    return 0;
}

/*
 * Parse root system description table (RSDT/XSDT)
 */
int
acpi_parse_rsdt(struct acpi_rsdp *rsdp)
{
    struct acpi_sdt_hdr *rsdt;
    int i;
    int nr;
    int sz;

    if ( rsdp->revision >= 1 ) {
        /* ACPI 2.0 or later */
        sz = 8;
        rsdt = (struct acpi_sdt_hdr *)rsdp->xsdt_addr;
        if ( 0 != cmp((u8 *)rsdt->signature, (u8 *)"XSDT ", 4) ) {
            return -1;
        }
    } else {
        /* Parse RSDT (ACPI 1.x) */
        sz = 4;
        rsdt = (struct acpi_sdt_hdr *)(u64)rsdp->rsdt_addr;
        if ( 0 != cmp((u8 *)rsdt->signature, (u8 *)"RSDT ", 4) ) {
            return -1;
        }
    }

    nr = (rsdt->length - sizeof(struct acpi_sdt_hdr)) / sz;
    for ( i = 0; i < nr; i++ ) {
        u64 xx = *(u64 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        struct acpi_sdt_hdr *tmp = (struct acpi_sdt_hdr *)xx;
        if ( 0 == cmp((u8 *)tmp->signature, (u8 *)"APIC", 4) ) {
            /* APIC */
            acpi_parse_apic(tmp);
        } else if ( 0 == cmp((u8 *)tmp->signature, (u8 *)"FACP", 4) ) {
            /* FADT */
            acpi_parse_fadt(tmp);
        }
    }

    return 0;
}

/*
 * Search Root System Description Pointer (RSDP) in ACPI
 */
int
acpi_rsdp_search_range(u64 start, u64 end)
{
    u64 addr;
    struct acpi_rsdp *rsdp;

    for ( addr = start; addr < end; addr += 0x10 ) {
        /* Check the checksum */
        if ( 0 == acpi_checksum((u8 *)addr, 20) ) {
            rsdp = (struct acpi_rsdp *)addr;
            if ( 0 == cmp((u8 *)rsdp->signature, (u8 *)"RSD PTR ", 8) ) {
                acpi_parse_rsdt(rsdp);
                return 1;
            }
        }
    }

    return 0;
}

/* Load Root System Description Pointer in ACPI */
int
acpi_load(void)
{
    u16 ebda;
    u64 ebda_addr;

    acpi_pm_tmr_port = 0;
    acpi_ioapic_base = 0;

    /* Check 1KB of EBDA, first */
    ebda = *(u16 *)0x040e;
    if ( ebda ) {
        ebda_addr = (u64)ebda << 4;
        if ( acpi_rsdp_search_range(ebda_addr, ebda_addr + 0x0400) ) {
            return 1;
        }
    }

    /* Check main BIOS area */
    return acpi_rsdp_search_range(0xe0000, 0x100000);
}

u32
acpi_get_timer(void)
{
    return inl(acpi_pm_tmr_port) & 0xffffff;
}

/*
 * Wait
 */
void
acpi_busy_usleep(u64 usec)
{
    u64 clk;
    u64 acc;
    u64 cur;
    u64 prev;

    /* usec to count */
    clk = (ACPI_TMR_HZ * usec) / 1000000;

    prev = acpi_get_timer();
    acc = 0;
    while ( acc < clk ) {
        cur = acpi_get_timer();
        if ( cur < prev ) {
            /* Overflow */
            acc += 0x1000000 + cur - prev;
        } else {
            acc += cur - prev;
        }
        prev = cur;
        pause();
    }
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
