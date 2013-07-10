/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "../../kernel.h"
#include "arch.h"
#include "desc.h"
#include "acpi.h"
#include "apic.h"
#include "i8254.h"

#define VGA_VRAM_TEXTMODE       0x000b8000
#define VGA_TEXTMODE_X          80
#define VGA_TEXTMODE_Y          25
#define VGA_TEXTMODE_XY         (VGA_TEXTMODE_X * VGA_TEXTMODE_Y)

struct bootinfo {
    struct sysaddrmap {
        u64 n;
        struct sysaddrmap_entry *entries;
    } sysaddrmap;
};
struct sysaddrmap_entry {
    u64 base;
    u64 len;
    u32 type;
    u32 attr;
};

static int cursor;
static unsigned char vga_text[VGA_TEXTMODE_XY];

static u64 cpu_freq;
static u64 fsb_freq;

void apic_test(void);
void search_clock_sources(void);


#define BOOTINFO_BASE 0x8000
#define TRAMPOLINE_ADDR         0x20000
#define TRAMPOLINE_MAX_SIZE     4096

void trampoline(void);
void trampoline_end(void);

void
arch_init(void)
{
    /* Find configuration using ACPI */
    acpi_load();

    /* Initialize interrupt descriptor table */
    idt_init();

    /* Set general protection fault handler */
    idt_setup_intr_gate(13, &intr_gpf);

    /* Initialize local APIC */
    //lapic_init();

    /* Initialize I/O APIC */


    /* Set IRQ0 */
    //idt_setup_intr_gate(32, &intr_lapic_int32);


    /* Stop i8254 timer */
    i8254_stop_timer();

    /* Initialize VGA display */
    cursor = 0;

    int i;

    /* IPI: 4KiB boundry */
    u64 tsz = trampoline_end - trampoline;
    if ( tsz > TRAMPOLINE_MAX_SIZE ) {
        /* Error */
        // panic
        return;
    }
    /* Copy trampoline */
    for ( i = 0; i < tsz; i++ ) {
        *(u8 *)((u64)TRAMPOLINE_ADDR + i) = *(u8 *)((u64)trampoline + i);
    }
    /* Get and clear P.2014 */
    u32 icrl;
    u32 icrh;
    /* INIT */
    __asm__ __volatile__ ("movq $0xfee00300,%%rdx; movl (%%rdx),%%eax; andl $~0xcdfff,%%eax" : "=a"(icrl) : );
    __asm__ __volatile__ ("movq $0xfee00310,%%rdx; movl (%%rdx),%%eax; andl $0x00ffffff,%%eax" : "=a"(icrh) : );
    icrl |= 0x0500; /* 101: INIT */
    icrl |= 0x4000; /* level = assert */
    /* triger = edge */
    /* destination = physical */
    /* destination shorthand = no shorthand */
    icrl |= (TRAMPOLINE_ADDR >> 12) & 0xff; /* Vector: FIXME assertion */
    icrh |= (1 << 24); /* Destination APIC ID */
    __asm__ __volatile__ ("movq $0xfee00300,%%rdx; movl %%eax,(%%rdx)" : : "a"(icrl) );
    __asm__ __volatile__ ("movq $0xfee00310,%%rdx; movl %%eax,(%%rdx)" : : "a"(icrh) );

    kprintf("ICR %.8x %.8x\r\n", icrh, icrl);
    arch_busy_usleep(1000000);

    /* STARTUP */
    __asm__ __volatile__ ("movq $0xfee00300,%%rdx; movl (%%rdx),%%eax; andl $~0xcdfff,%%eax" : "=a"(icrl) : );
    __asm__ __volatile__ ("movq $0xfee00310,%%rdx; movl (%%rdx),%%eax; andl $0x00ffffff,%%eax" : "=a"(icrh) : );
    icrl |= 0x0600; /* 110: Startup */
    icrl |= 0x4000; /* level = assert */
    /* triger = edge */
    /* destination = physical */
    /* destination shorthand = no shorthand */
    icrl |= (TRAMPOLINE_ADDR >> 12) & 0xff; /* Vector: FIXME assertion */
    icrh |= (1 << 24); /* Destination APIC ID */
    __asm__ __volatile__ ("movq $0xfee00300,%%rdx; movl %%eax,(%%rdx)" : : "a"(icrl) );
    __asm__ __volatile__ ("movq $0xfee00310,%%rdx; movl %%eax,(%%rdx)" : : "a"(icrh) );

    kprintf("ICR %.8x %.8x\r\n", icrh, icrl);
    arch_busy_usleep(1000);

    /* Startup again */
    __asm__ __volatile__ ("movq $0xfee00300,%%rdx; movl %%eax,(%%rdx)" : : "a"(icrl) );
    __asm__ __volatile__ ("movq $0xfee00310,%%rdx; movl %%eax,(%%rdx)" : : "a"(icrh) );
    arch_busy_usleep(1000);

    // $0xfee00300 &=~0xcdfff
    // $0xfee00310
    //__asm__ __volatile__ ("movq %%rax,%%dr0" :: "a"(taddr) );

    //kprintf("size %x\r\n", taddr);
    //__asm__ __volatile__ ("movq %%rax,%%dr0" :: "a"(taddr) );


    struct bootinfo *bi;
    bi = (struct bootinfo *)BOOTINFO_BASE;
    kprintf("SYSTEM ADDRESS MAP\r\n");
    for ( i = 0; i < bi->sysaddrmap.n; i++ ) {
        kprintf("%.16x %.16x %d\r\n", bi->sysaddrmap.entries[i].base,
                bi->sysaddrmap.entries[i].len, bi->sysaddrmap.entries[i].type);
    }
    kprintf("-----------------------------------\r\n");


    apic_test();

    char *str = "Welcome to AOS!  Now this message is printed by C function.";

    kprintf("%s\r\n", str);


    u64 x;
    /* MSR_PERF_STAT */
    x = rdmsr(0x198);
    kprintf("%.16x\r\n", x);
    /* MSR_PLATFORM_ID */
    x = rdmsr(0x17);
    kprintf("%.16x\r\n", x);
    /* MSR_PLATFORM_INFO */
    x = rdmsr(0xce);
    kprintf("%.16x\r\n", x);

    __asm__ __volatile__ ("movl $0x80000007,%%eax;cpuid;movq %%rdx,%%rax" : "=a"(x) : );
    kprintf("%.16x\r\n", x);

    __asm__ __volatile__ ("movl $0x1,%%eax;cpuid" : "=a"(x) : );
    kprintf("%.16x\r\n", x);

    /* MSR_FSB_FREQ */
    x = rdmsr(0xcd);
    kprintf("%.16x\r\n", x);

    search_clock_sources();

    kprintf("%.16x\r\n", acpi_pm_tmr_port);
    kprintf("%.16x\r\n", acpi_ioapic_base);

#if 0
    u64 tc0, tc1;
    u64 pc0, pc1;
    u32 t0, t1, t2;

    /* Get local APIC ID */
    //__asm__ __volatile__ ("movq $0xfee00000,%rdx; movl 0x020(%rdx),%eax; shr $24,%eax; andl $0xff,%eax");
    __asm__ __volatile__ ("movq $0xfee00000,%rdx; movl $0x10,0x340(%rdx)");
    __asm__ __volatile__ ("movq $0xfee00000,%rdx; movl $0,0x320(%rdx)");
    __asm__ __volatile__ ("movq $0xfee00000,%rdx; movl $3,0x3e0(%rdx)");
    __asm__ __volatile__ ("movq $0xfee00000,%rdx; movl $0xffffffff,0x380(%rdx)");

    t1 = inl(acpi_pm_tmr_port) & 0xffffff;
    tc0 = rdtsc();
    __asm__ __volatile__ ("movq $0xfee00000,%%rdx; movl 0x390(%%rdx),%%eax" : "=a"(pc0) : );

    kprintf("TMR: %d\r\n", t1);
    int cnt = 0;
    t0 = t1;
    while ( cnt < 2 ) {
        t2 = inl(acpi_pm_tmr_port) & 0xffffff;
        if ( t1 > t2 ) {
            kprintf("TMR: %d\r\n", t1);
            cnt++;
        }
        t1 = t2;
        pause();
    }
    u64 hz = 3579545;
    int duration = (0x2000000 - t0);
    tc1 = rdtsc();

    __asm__ __volatile__ ("movq $0xfee00000,%%rdx; movl 0x390(%%rdx),%%eax" : "=a"(pc1) : );
    __asm__ __volatile__ ("movq $0xfee00000,%rdx; movl $0x10,0x320(%rdx)");


    kprintf("CPU FREQ: %lld Hz\r\n", (tc1 - tc0) * hz / duration);
    kprintf("BUS FREQ: %lld Hz %.16x %.16x\r\n", 16 * (pc0 - pc1) * hz / duration, pc1, pc0);

    arch_busy_usleep(1000000);
    kprintf("Test\r\n");
#endif

#if 0

    u64 tsc1;
    u64 tsc2;
    vga_putc('x');
    vga_putc('/');
    tsc1 = rdtsc();
    for ( ;; ) {
        tsc2 = rdtsc();
        if ( tsc2 - tsc1 > 266660000000 ) {
            break;
        }
        pause();
    }
    vga_putc('y');
#endif


}


/*
 * Update cursor
 */
void
vga_update_cursor(void)
{
    u16 val;
    u16 addr = 0x3d4;

    /* Low */
    val = ((cursor & 0xff) << 8) | 0x0f;
    outw(addr, val);
    //__asm__ __volatile__ ( "outw %%ax,%%dx" : : "a"(val), "d"(addr) );
    /* High */
    val = (((cursor >> 8) & 0xff) << 8) | 0x0e;
    outw(addr, val);
    //__asm__ __volatile__ ( "outw %%ax,%%dx" : : "a"(val), "d"(addr) );
}

/*
 * Put a character to VGA display
 */
void
vga_putc(int c)
{
    int i;
    u64 addr;
    u16 *ptr;

    addr = VGA_VRAM_TEXTMODE;

    if ( '\r' == c ) {
        /* Carriage return */
        cursor = cursor / VGA_TEXTMODE_X * VGA_TEXTMODE_X;
    } else if ( '\n' == c ) {
        /* New line */
        cursor = cursor + VGA_TEXTMODE_X;
    } else {
        /* Other characters */
        ptr = (u16 *)(addr + (cursor) * 2);
        *ptr = (0x07 << 8) | (u8)c;
        vga_text[cursor] = c;
        cursor++;
    }

    /* Draw it again */
    if ( cursor >= VGA_TEXTMODE_XY ) {
        for ( i = 0; i < VGA_TEXTMODE_XY - VGA_TEXTMODE_X; i++ ) {
            ptr = (u16 *)(addr + i * 2);
            vga_text[i] = vga_text[i + VGA_TEXTMODE_X];
            *ptr = (0x07 << 8) | vga_text[i];
        }
        for ( ; i < VGA_TEXTMODE_XY; i++ ) {
            ptr = (u16 *)(addr + i * 2);
            vga_text[i] = ' ';
            *ptr = (0x07 << 8) | vga_text[i];
        }
        cursor -= VGA_TEXTMODE_X;
    }
    vga_update_cursor();
}

/*
 * Compute TSC frequency
 */
u64
calib_tsc_freq(void)
{
    return 0;
}

u64
get_invariant_tsc_frequency(void)
{
    u64 val;
    int family;
    int model;

    /* Check invariant TSC support first */
    if ( !is_invariant_tsc() ) {
        /* Invariant TSC is not available */
        return 0;
    }

    /* MSR_PLATFORM_INFO */
    val = rdmsr(0xce);
    /* Get maximum non-turbo ratio [15:8] */
    val = (val >> 8) & 0xff;

    family = get_cpu_family();
    model = get_cpu_model();
    if ( 0x06 == family && (0x2a == model || 0x2d == model || 0x3a == model) ) {
        /* SandyBridge (06_2AH, 06_2DH) and IvyBridge (06_3AH) */
        val *= 100000000; /* 100 MHz */
    } else if ( 0x06 == family
                && (0x1a == model || 0x1e == model || 0x1f == model
                    || 0x25 == model || 0x2c == model || 0x2e == model
                    || 0x2f == model) ) {
        /* Nehalem (06_1AH, 06_1EH, 06_1FH, 06_2EH)
           and Westmere (06_2FH, 06_2CH, 06_25H) */
        val *= 133330000; /* 133.33 MHz */
    } else {
        /* Unknown model */
        val = 0;
    }

    return val;
}

/*
 * Search clock sources
 */
void
search_clock_sources(void)
{
    u64 tscfreq;

    /* Get invariant TSC frequency */
    tscfreq = get_invariant_tsc_frequency();
    if ( tscfreq ) {
        kprintf("Freq: %.16x\r\n", tscfreq);
    }

    /* Get ACPI info */
}


void
arch_putc(int c)
{
    vga_putc(c);
}

void
arch_get_tmr(u64 counter, u64 *resolution)
{
}

/* Wait X microsecond */
void
arch_busy_usleep(u64 usec)
{
    acpi_busy_usleep(usec);
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
