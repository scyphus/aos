#
# $Id$
#
# Copyright (c) 2013 Scyphus Solutions. All rights reserved.
# Authors:
#      Hirochika Asai  <asai@scyphus.co.jp>
#

ARCH=x86_64

diskboot: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
bootmon: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
kpack: ASFLAGS=-nostdlib
kpack: CFLAGS=-Wall -mno-sse2 -mno-sse3 -fleading-underscore -nostdlib -m64

## MBR
diskboot: boot/arch/$(ARCH)/diskboot.o
	$(LD) -N -e start -Ttext=0x7c00 --oformat binary -o $@ $^

bootmon: boot/arch/$(ARCH)/bootmon.o \
	boot/arch/$(ARCH)/entry32.o \
	boot/arch/$(ARCH)/entry64.o
	$(LD) -N -e bootmon -Ttext=0x09000 --oformat binary -o $@ $^

kpack: kernel/arch/$(ARCH)/asm.o kernel/kernel.o
	$(LD) -N -e _kmain -Ttext=0x10000 --oformat binary -o $@ $^

.s.o:
	$(AS) $(ASFLAGS) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	find . -name "*.o" | xargs rm -f

.SUFFIX: .c .s .o

