/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#ifndef _BOOT_H
#define _BOOT_H

#include <aos/const.h>

#define PAGESIZE                4096
#define PHYS_MEM_FREE_ADDR      0x100000

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
};
struct pci {
    struct pci_device *device;
    struct pci *next;
};


/* Block device */
typedef int (*read_f)(void *);
typedef int (*write_f)(void *);
struct block_dev {
    void *user;
    read_f read;
    write_f write;
};

#ifdef __cplusplus
extern "C" {
#endif

    /* Defined in entry64.s */
    void ljmp64(void *);
    u32 inl(u16);
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
