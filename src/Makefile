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
kpack: ASFLAGS=-nostdlib -I./kernel/arch/$(ARCH)/
kpack: CFLAGS=-I./include \
	-Wall -mno-sse3 -fleading-underscore -O3 -nostdlib -m64

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
	boot/arch/$(ARCH)/boot.o \
	boot/arch/$(ARCH)/memory.o \
	boot/arch/$(ARCH)/vga.o \
	boot/arch/$(ARCH)/pci.o \
	boot/arch/$(ARCH)/ahci.o \
	boot/arch/$(ARCH)/ohci.o \
	boot/arch/$(ARCH)/xhci.o \
	boot/arch/$(ARCH)/fat.o
	$(LD) -N -e bootmon -Ttext=0x09000 --oformat binary -o $@ $^

bootx64.efi: efi/arch/$(ARCH)/efi.o
	$(LD) -N -e efimain -Ttext=0x7000 --oformat binary -o $@ $^

## Kernel
kpack: kernel/arch/$(ARCH)/asm.o \
	kernel/kernel.o \
	kernel/router.o \
	kernel/util.o \
	kernel/shell.o \
	kernel/task.o \
	kernel/arch/$(ARCH)/arch.o \
	kernel/arch/$(ARCH)/spinlock.o \
	kernel/arch/$(ARCH)/kbd.o \
	kernel/arch/$(ARCH)/vga.o \
	kernel/arch/$(ARCH)/acpi.o \
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
	kernel/ptcam.o \
	drivers/rng/rng.o \
	drivers/net/net.o \
	drivers/net/netdev.o \
	drivers/e1000/e1000.o \
	drivers/e1000e/e1000e.o \
	drivers/ixgbe/ixgbe.o \
	drivers/ahci/ahci.o \
	drivers/pci/pci.o
	$(LD) -N -e kstart64 -Ttext=0x10000 --oformat binary -o $@ $^

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

