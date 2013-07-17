# Academic Operating System

## About
We are developing an operating system for practical education.  This
motivation is similar to MINIX, but we do not focus on theories.
Our main objective is to provide knowledges on hardware-related
programming.  This is one of the most difficult and complex parts
when we start the development of operating system from scratch.

## Developer
Hirochika Asai

## ToDo
- Somehow convert local APIC ID to CPU ID (using ACPI table)
### Future work
- Design and implement glue between the boot monitor and the kernel.
- Implement a simple disk driver in the boot monitor to load the
  kernel (to higher address space).  Note that it's now implemented
  with BIOS function in boot/arch/x86_64/bootmon.s.

## Checkpoints
- boot_cp0.1: Simple boot example (just printing a welcome message
  and stop CPU)
- boot_cp0.2: Load boot monitor from MBR and jump there (and then
  immediately shutoff the machine using APM)
- boot_cp0.3: Get into protected mode
- boot_cp0.4: Get into long mode from boot monitor
- boot_cp0.5: Load kernel and execute the program

## Physical memory map
    Start    End       Description
    ----------------------------------------------------------
    00000500 00007bff  Default stack in boot monitor
    00007c00 00007dff  MBR
    00007e00 00007fff  free
    00008000 00008fff  boot information
    00009000 00009fff  boot monitor
    0000a000 0000ffff  free (reserved for boot monitor)
    ----------------------------------------------------------
    00010000 00017fff  kernel
    00018000 0001ffff  free (reserved for kernel)
    00020000 00078fff  free
    00070000 00073fff  trampoline (16KiB)
    00074000 00077fff  GDT, IDT (20KiB)
    00078000 00079fff  free
    00079000 0007ffff  page table (at least 24KiB = 6 * 4KiB)
    00080000 000fffff  free or reserved (We don't use here)
    ----------------------------------------------------------
    00100000 00ffffff  free (fore memory management?)
    ----------------------------------------------------------
    01000000 0100ffff  BSP (flags, tss, stack)
    01010000 0101ffff  AP #1
    ....     01ffffff  for processors
    ----------------------------------------------------------
    02000000 --------  memory

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
        u64 base;
        u64 len;
        u32 type;
        u32 attr;
    }

## Interrupt Service Routine
  20 IRQ0  I/O APIC timer
  21 IRQ1  keyboard
  50 Local APIC timer
  fe IPI Panic
  ff Spurious interrupt vector



## Memo

### Steps
1. Jump to 0000:7c00 (16 bit mode) from BIOS
2. Load boot monitor from MBR (IPL)
3. Load kernel from boot monitor
4. Enter protected mode then long mode (kstart64)
5. Call C function (kmain)
   Load GDT, IDT, and TSS
   Initialize some devices
6. Load trampoline and boot APs
7. Enter protected mode and long mode (apstart64)
8. Call C function (apstart64)
   Load GDT, IDT and TSS

### Multiprocessor / Multicore system

#### Trampoline
1. send an INIT IPI
   wait 10ms
2. send a START IPI
   wait 200us
3. send another START IPI
   wait 200us
4. trampoline at the processor we are starting
-- %cs == vector # specified in ICR
-- %ip == 0
-- %ds == 0

#### Devices per-processor or per-host
##### Per-processor (core)
- Registers
- Local APIC
- GDTR
- IDTR
- TSS (TR)
- Stack
- Page table (?)

##### Per-host
- Memory
- I/O APIC
- i8254 (Programmable Interval Timer)
- i8259 (Programmable Interrupt Controller)
- GDT
- IDT

### Tips
- `movl data,%regl' clears most significant 32 bits of %regl
