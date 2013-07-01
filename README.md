# Academic Operating System

## About
We are developing an operating system for practical education.  This
motivation is similar to MINIX, but we do not focus on theories.
Our main objective is to provide knowledges on hardware-related
programming.  This is one of the most difficult and complex parts
when we start the development of operating system from scratch.

## Developer
Hirochika Asai

## Checkpoints
- boot_cp0.1: Simple boot example (just printing a welcome message and stop CPU)
- boot_cp0.2: Load kernel loader from MBR and jump there (and then immediately shutoff the machine using APM)
- boot_cp0.3: Get into protected mode

## Physical memory map
    Start    End       Description
    ----------------------------------------------------------
    00000500 00007bff  Default stack in real mode
    00007c00 00007dff  MBR
    00007e00 00007fff  free
    00008000 00008fff  boot information
    00009000 00009fff  boot monitor
    0000a000 0000ffff  free (reserved for boot monitor)
    00010000 0001ffff  kernel
    00020000 00078fef  kernel stack
    00078ff0 00078fff  16 byte free range
    00079000 0007ffff  page table (at least 24KiB = 6 * 4KiB)
    00080000 000fffff  free or reserved

### Boot information structure
    /* The size of boot information must be aligned on 4 byte boundaries */
    /* 0x0000--0x000f */
    struct sysaddrmap {
        u64 num;               /* Number of entries */
        u64 *sysaddrmap_entry; /* Pointer to system address map entries */
    }
    /* 0x0010--0x0100 */
    /* reserved for future use */

#### System address map entry
    struct sysaddrmap_entry {
        u64 basee;
        u64 len;
        u32 type;
        u32 attr;
    }





