# PIX

## About
We are developing an operating system, called PIX, for my research
and practical education.
For the academic purpose, this motivation is similar to MINIX,
but we do not focus on theories.
Our main objective is to provide knowledges on hardware-related
programming.  This is one of the most difficult and complex parts
when we start the development of operating system from scratch.

## System requirements
- Memory: >= 32MiB


## Developer
Hirochika Asai

## ToDo
- Somehow convert local APIC ID to CPU ID (using ACPI table)
- Implement callout queue
- Support FAT16/32 in boot monittor
- Setup BSP in kernel (not reuse the boot monitor's one)

## Physical memory map
### Boot monitor
    Start    End       Description
    ----------------------------------------------------------
    00000500 00007bff  Default stack in boot monitor
    00007c00 00007dff  MBR
    00007e00 00007fff  free
    00008000 00008fff  boot information
    00009000 0000cfff  boot monitor (16KiB)
    0000d000 0000ffff  free (reserved for boot monitor)
    ----------------------------------------------------------
    00010000 00079fff  free
    00079000 0007ffff  page table (at least 24KiB = 6 * 4KiB)
    00080000 000fffff  free or reserved (We don't use here)
    ----------------------------------------------------------
    00100000 00ffffff  reserved for kernel
    01000000 01ffffff  for processor flags, tss, stack
    ----------------------------------------------------------
    * managed by the memory allocator below *
    ----------------------------------------------------------
    02000000 --------  free

### Kernel
    Start    End       Description
    ----------------------------------------------------------
    00000500 00007fff  free
    00008000 00008fff  boot information
    00009000 0000ffff  free
    ----------------------------------------------------------
    00010000 0001ffff  kernel
    00020000 00078fff  initpack (ramfs)
    00070000 00073fff  trampoline (16KiB)
    00074000 00077fff  GDT, IDT (20KiB)
    00078000 00079fff  free
    00079000 0007ffff  page table (at least 24KiB = 6 * 4KiB)
    00080000 000fffff  free or reserved (We don't use here)
    ----------------------------------------------------------
    00100000 00ffffff  free
    ----------------------------------------------------------
    01000000 0100ffff  BSP (flags, tss, stack)
    01010000 0101ffff  AP #1
    ....     01ffffff  for processors
    ----------------------------------------------------------
    * managed by the memory allocator below *
    ----------------------------------------------------------
    02000000 --------  free


### Page table
    Start    End       Description
    ----------------------------------------------------------
    00000000 3fffffff  Kernel
    40000000 --------  User land


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

## Disk usage
### GPT
    Start    End       Description
    ----------------------------------------------------------
    00000000 000001ff  LBA 0
    00000200 000043ff  LBA 1-34
    00004400 0000ffff  Partition 1: Stage 2 (binary)
    00010000 --------  Partition 2: Filesystem (FAT)
    ----------------------------------------------------------

### MBR
    Start    End       Description
    ----------------------------------------------------------
    00000000 000001ff  MBR
    00000200 0000ffff  Stage 2 (binary)
    00010000 --------  Partition 1: Filesystem (FAT)
    ----------------------------------------------------------


## Interrupt Service Routine
  0--15 reserved by processor
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
- PCI database: http://www.pcidatabase.com

## Supported devices
### Ethernet cards
    8086:100e Intel Pro 1000/MT
    8086:100f Intel 82545EM
    8086:107c Intel 82541PI
    8086:109a Intel 82573L
    8086:10f5 Intel 82567LM
    8086:10ea Intel 82577LM
    8086:1502 Intel 82579LM
    8086:1559 Intel I218-V
    8086:10fb Intel X520
    8086:1583 Intel XL710-QDA2
    8086:1584 Intel XL710-QDA1

### AHCI
    8086:2829 Intel 82801HBM
    8086:1e02 Intel Corporation 7 Series/C210 Series Chipset Family 6-port SATA Controller
    8086:8c02 Intel Corporation 8 Series/C220 Series Chipset Family 6-port SATA Controller

## Task tree

* (-1) idle
* (0) kmain (kernel)
* (1) init (launcher)
** driver
** shell
