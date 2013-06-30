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
- boot_cp0.1: Simple boot example (just printing a welcome message and stop CPU.)
- boot_cp0.2: Load kernel loader from MBR and jump there (and then immediately shutoff the machine using APM)

## Physical memory map
    Start    End       Description
    ---------------------------------------------------------
    00000500 00007bff  Stack in real mode
    00007c00 00007dff  MBR
    00007e00 00007fff  free
    00008000 00008fff  boot information
    00009000 00009fff  kernel loader
    0000a000 0000ffff  free (researved for kernel loader)
    00010000 0001ffff  kernel

### Boot information structure
    struct sysaddrmap {
        u64 num; /* (number of entries) */
        u64 *sysaddrmap_entry; /* (pointer to system address map entries) */
    }

#### System address map entry
    struct sysaddrmap_entry {
        u64 basee;
        u64 len;
        u32 type;
        u32 attr;
    }





