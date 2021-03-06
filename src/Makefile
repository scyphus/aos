#
# Copyright (c) 2013 Scyphus Solutions Co. Ltd.
# Copyright (c) 2014 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

ARCH=x86_64

diskboot: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
bootmon: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
bootmon: CFLAGS=-I./include \
	-Wall -mno-sse3 -fleading-underscore -nostdlib -m64
bootx64.efi: ASFLAGS=-nostdlib -I./kernel/arch/$(ARCH)/
bootx64.efi: CFLAGS=-Wall -mno-sse3 -fleading-underscore -nostdlib -m64
pxeboot: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
kpack: ASFLAGS=-nostdlib -I./kernel/arch/$(ARCH)/
kpack: CFLAGS=-I./include \
	-Wall -fleading-underscore -nostdlib -O3 -m64

## MBR
diskboot: boot/arch/$(ARCH)/diskboot.o
	$(LD) -N -e start -Ttext=0x7c00 --oformat binary -o $@ $^

## Boot monitor
boot/arch/$(ARCH)/bootmon.o: boot/arch/$(ARCH)/asmconst.h
boot/arch/$(ARCH)/entry16.o: boot/arch/$(ARCH)/asmconst.h
boot/arch/$(ARCH)/entry32.o: boot/arch/$(ARCH)/asmconst.h
boot/arch/$(ARCH)/entry64.o: boot/arch/$(ARCH)/asmconst.h
bootmon: boot/arch/$(ARCH)/bootmon.o \
	boot/arch/$(ARCH)/entry16.o \
	boot/arch/$(ARCH)/entry32.o \
	boot/arch/$(ARCH)/entry64.o \
	boot/arch/$(ARCH)/boot.o
	$(LD) -N -e bootmon -Ttext=0x09000 --oformat binary -o $@ $^

bootx64.efi: efi/arch/$(ARCH)/efi.o
	$(LD) -N -e efimain -Ttext=0x7000 --oformat binary -o $@ $^


## PXE boot
pxeboot: boot/arch/$(ARCH)/pxeboot.o
	$(LD) -N -e pxeboot -Ttext=0x7c00 --oformat binary -o $@ $^

## Kernel
kpack: kernel/arch/$(ARCH)/asm.o \
	kernel/kernel.o \
	kernel/processor.o \
	kernel/util.o \
	kernel/shell.o \
	kernel/task.o \
	kernel/system.o \
	kernel/mgmt.o \
	kernel/arch/$(ARCH)/arch.o \
	kernel/arch/$(ARCH)/spinlock.o \
	kernel/arch/$(ARCH)/vga.o \
	kernel/arch/$(ARCH)/acpi.o \
	kernel/arch/$(ARCH)/acpi_dsdt.o \
	kernel/arch/$(ARCH)/apic.o \
	kernel/arch/$(ARCH)/clock.o \
	kernel/arch/$(ARCH)/trampoline.o \
	kernel/arch/$(ARCH)/ap_entry32.o \
	kernel/arch/$(ARCH)/ap_entry64.o \
	kernel/arch/$(ARCH)/desc.o \
	kernel/arch/$(ARCH)/memory.o \
	kernel/arch/$(ARCH)/console.o \
	kernel/arch/$(ARCH)/cmos.o \
	kernel/arch/$(ARCH)/i8254.o \
	kernel/netsc.o \
	drivers/kbd/kbd.o \
	drivers/rng/rng.o \
	drivers/net/net.o \
	drivers/net/tcp.o \
	drivers/net/netdev.o \
	drivers/e1000/e1000.o \
	drivers/e1000e/e1000e.o \
	drivers/ixgbe/ixgbe.o \
	drivers/i40e/i40e.o \
	drivers/ahci/ahci.o \
	drivers/pci/pci.o \
	kernel/ptcam.o \
	kernel/dxr.o \
	kernel/mbt.o \
	kernel/buddy.o \
	kernel/sail.o
	$(LD) -N -e kstart64 -Ttext=0x10000 --oformat binary -o $@ $^

#drivers/net/kuhash.o: CFLAGS=-I./include \
#	-Wall -fleading-underscore -nostdlib -O0 -m64

## Dependencies
kernel/kernel.o: kernel/kernel.h
kernel/arch/$(ARCH)/arch.o: kernel/arch/$(ARCH)/arch.h \
	kernel/arch/$(ARCH)/apic.h
kernel/arch/$(ARCH)/apic.o: kernel/arch/$(ARCH)/apic.h


.s.o:
	$(AS) $(ASFLAGS) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	find . -name "*.o" | xargs rm -f

.SUFFIX: .c .s .o

