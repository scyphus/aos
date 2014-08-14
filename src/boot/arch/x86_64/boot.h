/*_
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#ifndef _BOOT_H
#define _BOOT_H

#include <aos/const.h>

#define BOOTINFO_ADDR           0x00008000

#define PAGESIZE                4096
#define PHYS_MEM_FREE_ADDR      0x2000000

typedef __builtin_va_list va_list;
#define va_start(ap, last)      __builtin_va_start((ap), (last))
#define va_arg                  __builtin_va_arg
#define va_end(ap)              __builtin_va_end(ap)
#define va_copy(dest, src)      __builtin_va_copy((dest), (src))
#define alloca(size)            __builtin_alloca((size))

/* Boot information */
struct bootinfo {
    struct sysaddrmap {
        u64 n;
        struct bootinfo_sysaddrmap_entry *entries;
    } sysaddrmap;
};
struct bootinfo_sysaddrmap_entry {
    u64 base;
    u64 len;
    u32 type;
    u32 attr;
};

/* pci.c */
struct pci_device {
    u16 bus;
    u16 slot;
    u16 func;
    u16 vendor_id;
    u16 device_id;
    u8 intr_pin;        /* 0x01: INTA#, 0x02: INTB#, 0x03: INTC#: 0x04: INTD# */
    u8 intr_line;       /* 0xff: no connection */
    u8 class;
    u8 subclass;
    u8 progif;
};
struct pci {
    struct pci_device *device;
    struct pci *next;
};


/* Block device */
struct blkdev;
typedef int (*blk_read_f)(struct blkdev *, u64, u32, u8 *);
typedef int (*blk_write_f)(struct blkdev *, u64, u32, u8 *);
struct blkdev {
    blk_read_f read;
    blk_write_f write;
    void *parent;
};

struct blkdev_list {
    struct blkdev *blkdev;
    struct blkdev_list *next;
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Defined in entry64.s */
    void ljmp64(void *);
    u32 inl(u16);
    void outw(u16, u16);
    void outl(u16, u32);
    void spin_lock(int *);
    void spin_unlock(int *);

    /* Defined in memory.c */
    int memory_init(struct bootinfo *);
    void * bmalloc(u64);
    void bfree(void *);

    /* Defined in pci.c */
    void pci_init(void);
    u16 pci_read_config(u16, u16, u16, u16);
    u64 pci_read_mmio(u8, u8, u8);
    struct pci * pci_list(void);

    /* Defined in ahci.c */
    void ahci_init(void);

    /* Defined in ohci.c */
    void ohci_init(void);

    /* Defined in fat.c */
    int fat_load_kernel(struct blkdev *, u64);

    /* vga.c */
    void vga_init(void);
    int kprintf(const char *, ...);

#ifdef __cplusplus
}
#endif

#endif /* _BOOT_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
