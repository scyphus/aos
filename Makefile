#
# $Id$
#
# Copyright (c) 2013-2014 Scyphus Solutions. All rights reserved.
# Authors:
#      Hirochika Asai  <asai@scyphus.co.jp>
#

all:
	@echo "make all is not supported."

fdtool:
	make -C tools/fdtool fdtool

efitool:
	make -C tools/efitool efitool

tools: fdtool efitool

fdimage:
	@echo "Make sure tools are compiled."
	make -C src diskboot
	make -C src bootmon
	make -C src bootx64.efi
	make -C src kpack
	./tools/fdtool/fdtool ./aos.img src/diskboot src/bootmon src/kpack
	./tools/efitool/efitool ./aos.efi ./src/bootx64.efi
#	qemu-img convert -f raw -O vdi ./aos.img ./aos.vdi

image: fdimage

clean-image:
	make -C src clean

clean:
	make -C tools/fdtool clean
	make -C src clean
