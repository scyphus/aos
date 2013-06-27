#
# $Id$
#
# Copyright (c) 2013 Scyphus Solutions. All rights reserved.
# Authors:
#      Hirochika Asai  <asai@scyphus.co.jp>
#

ARCH=x86_64
ASFLAGS=-nostdlib
CFLAGS=-Wall -mno-sse2 -mno-sse3 -fleading-underscore -nostdlib -m64

## MBR
diskboot: boot/arch/$(ARCH)/diskboot.o
	$(LD) -N -e start -Ttext=0x7c00 --oformat binary -o $@ $^

loader: boot/arch/$(ARCH)/loader.o
	$(LD) -N -e loader -Ttext=0x90000 --oformat binary -o $@ $^

kpack: kernel/kernel.o
	$(LD) -N -e _kmain -Ttext=0x100000 --oformat binary -o $@ $^

.s.o:
	$(AS) $(ASFLAGS) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS)  -c -o $@ $<

clean:
	find . -name "*.o" | xargs rm -f

.SUFFIX: .c .s .o

