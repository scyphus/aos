/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include <aos/const.h>
#include "acpi.h"
#include "arch.h"

u64 acpi_ioapic_base;
u64 acpi_pm_tmr_port;
u8 acpi_pm_tmr_ext;
u32 acpi_pm1a_ctrl_block;
u32 acpi_pm1b_ctrl_block;
u16 acpi_slp_typa;
u16 acpi_slp_typb;
u32 acpi_smi_cmd_port;
u8 acpi_enable_val;
u8 acpi_cmos_century;

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
acpi_parse_dsdt(struct acpi_sdt_hdr *sdt)
{
    u32 len;
    u8 *addr;
    u8 sz;

    len = sdt->length;
    addr = (u8 *)((u64)sdt + sizeof(struct acpi_sdt_hdr));

    /* Parse DSDT content */
    acpi_parse_dsdt_root(addr, len);

    /* Search \_S5 package in the DSDT */
    while ( len >= 5 ) {
        if ( 0 == cmp(addr, (u8 *)"_S5_ ", 4) ) {
            break;
        }
        addr++;
        len--;
    }
    if ( len >= 5 ) {
        /* S5 was found */
        if ( 0x08 != *(addr-1) && !(0x08 == *(addr-2) && '\\' == *(addr-1)) ) {
            /* Invalid AML (0x08 = NameOp) */
            return -1;
        }
        if ( 0x12 != *(addr+4) ) {
            /* Invalid AML (0x12 = PackageOp) */
            return -1;
        }

        /* Skip _S5_\x12 */
        addr += 5;
        len -= 5;

        /* Calculate the size of the packet length */
        sz = (*addr & 0xc0) >> 6;
        /* Check the length and skip packet length and the number of elements */
        if ( len < sz + 2 ) {
            return -1;
        }
        addr += sz + 2;
        len -= sz + 2;

        /* SLP_TYPa */
        if ( 0x0a == *addr ) {
            /* byte prefix */
            if ( len < 1 ) {
                return -1;
            }
            addr++;
            len--;
        }
        if ( len < 1 ) {
            return -1;
        }
        acpi_slp_typa = (*addr) << 10;
        addr++;
        len--;

        /* SLP_TYPb */
        if ( 0x0a == *addr ) {
            /* byte prefix */
            if ( len < 1 ) {
                return -1;
            }
            addr++;
            len--;
        }
        if ( len < 1 ) {
            return -1;
        }
        acpi_slp_typb = (*addr) << 10;
        addr++;
        len--;

        return 0;
    } else {
        /* Not found */
        return -1;
    }

    return -1;
}

int
acpi_parse_fadt(struct acpi_sdt_hdr *sdt)
{
    u64 addr;
    struct acpi_sdt_fadt *fadt;
    u32 len;
    u64 dsdt;

    len = 0;
    addr = (u64)sdt;
    len += sizeof(struct acpi_sdt_hdr);
    fadt = (struct acpi_sdt_fadt *)(addr + len);

    if ( sdt->revision >= 3 ) {
        /* FADT revision 2.0 or higher */
        if ( fadt->x_pm_timer_block.addr_space == 1 ) {
            /* Must be 1 (System I/O) */
            acpi_pm_tmr_port = fadt->x_pm_timer_block.addr;
            if ( !acpi_pm_tmr_port ) {
                acpi_pm_tmr_port = fadt->pm_timer_block;
            }
        }

        /* PM1a control block */
        if ( fadt->x_pm1a_ctrl_block.addr_space == 1 ) {
            /* Must be 1 (System I/O) */
            acpi_pm1a_ctrl_block = fadt->x_pm1a_ctrl_block.addr;
            if ( !acpi_pm1a_ctrl_block ) {
                acpi_pm1a_ctrl_block = fadt->pm1a_ctrl_block;
            }
        }

        /* PM1b control block */
        if ( fadt->x_pm1b_ctrl_block.addr_space == 1 ) {
            /* Must be 1 (System I/O) */
            acpi_pm1b_ctrl_block = fadt->x_pm1b_ctrl_block.addr;
            if ( !acpi_pm1b_ctrl_block ) {
                acpi_pm1b_ctrl_block = fadt->pm1b_ctrl_block;
            }
        }

        /* DSDT */
        dsdt = fadt->x_dsdt;
        if ( !dsdt ) {
            dsdt = fadt->dsdt;
        }
    } else {
        /* Revision  */
        acpi_pm_tmr_port = fadt->pm_timer_block;

        /* PM1a control block  */
        acpi_pm1a_ctrl_block = fadt->pm1a_ctrl_block;

        /* PM1b control block  */
        acpi_pm1b_ctrl_block = fadt->pm1b_ctrl_block;

        /* DSDT */
        dsdt = fadt->dsdt;
    }

    /* Check flags */
    acpi_pm_tmr_ext = (fadt->flags >> 8) & 0x1;

    /* SMI command */
    acpi_smi_cmd_port = fadt->smi_cmd_port;

    /* ACPI enable */
    acpi_enable_val = fadt->acpi_enable;

    /* Century */
    acpi_cmos_century = fadt->century;

    /* Parse DSDT */
    acpi_parse_dsdt((struct acpi_sdt_hdr *)dsdt);

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
    /* FIXME: 4byte --> 32bit */
    nr = (rsdt->length - sizeof(struct acpi_sdt_hdr)) / sz;
    for ( i = 0; i < nr; i++ ) {
        u64 xx;
        if ( 4 == sz ) {
            xx = *(u32 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        } else {
            xx = *(u64 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        }
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
    acpi_pm_tmr_ext = 0;
    acpi_ioapic_base = 0;
    acpi_pm1a_ctrl_block = 0;
    acpi_pm1b_ctrl_block = 0;
    acpi_slp_typa = 0;
    acpi_slp_typb = 0;
    acpi_smi_cmd_port = 0;
    acpi_enable_val = 0;

    /* Check 1KB of EBDA, first */
    ebda = *(u16 *)0x040e;
    if ( ebda ) {
        ebda_addr = (u64)ebda << 4;
        kprintf("*****EBDA %llx\r\n", ebda_addr);
        if ( acpi_rsdp_search_range(ebda_addr, ebda_addr + 0x0400) ) {
            return 1;
        }
    }

    /* Check main BIOS area */
    return acpi_rsdp_search_range(0xe0000, 0x100000);
}

/*
 * Get the current ACPI timer
 */
volatile u32
acpi_get_timer(void)
{
    return inl(acpi_pm_tmr_port);
}
u64
acpi_get_timer_period(void)
{
    if ( acpi_pm_tmr_ext ) {
        return ((u64)1<<32);
    } else {
        return (1<<24);
    }
}
u64
acpi_get_timer_hz(void)
{
    return ACPI_TMR_HZ;
}

/*
 * Wait
 */
void
acpi_busy_usleep(u64 usec)
{
    u64 clk;
    volatile u64 acc;
    volatile u64 cur;
    volatile u64 prev;

    /* usec to count */
    clk = (ACPI_TMR_HZ * usec) / 1000000;

    prev = acpi_get_timer();
    acc = 0;
    while ( acc < clk ) {
        cur = acpi_get_timer();
        if ( cur < prev ) {
            /* Overflow */
            acc += acpi_get_timer_period() + cur - prev;
        } else {
            acc += cur - prev;
        }
        prev = cur;
        pause();
    }
}

/*
 * Enable ACPI
 */
int
acpi_enable(void)
{
    if ( (inw(acpi_pm1a_ctrl_block) & ACPI_SCI_EN) == 0 ) {
        /* Enable ACPI */
        outb(acpi_smi_cmd_port, acpi_enable_val);

        if ( 0 == acpi_smi_cmd_port && 0 == acpi_enable_val ) {
            /* ACPI cannot enable */
            return -1;
        }
        /* Enable ACPI */
        outb(acpi_smi_cmd_port, acpi_enable_val);

        if ( (inw(acpi_pm1a_ctrl_block) & ACPI_SCI_EN) == 0 ) {
            /* Failed to enable ACPI */
            return -1;
        }
    }

    return 0;
}

/*
 * Power off the machine
 */
int
acpi_poweroff(void)
{
    int i;

    if ( 0 != acpi_enable() ) {
        return -1;
    }

    /* TYPa */
    if ( acpi_pm1a_ctrl_block && acpi_slp_typa ) {
        /* Try 30 times in 3 seconds */
        for ( i = 0; i < 30; i++ ) {
            outw(acpi_pm1a_ctrl_block, acpi_slp_typa | ACPI_SLP_EN);
            acpi_busy_usleep(100000);
        }
    }
    /* TYPb */
    if ( acpi_pm1b_ctrl_block && acpi_slp_typb ) {
        /* Try 30 times in 3 seconds */
        for ( i = 0; i < 30; i++ ) {
            outw(acpi_pm1b_ctrl_block, acpi_slp_typb | ACPI_SLP_EN);
            acpi_busy_usleep(100000);
        }
    }

    return -1;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
