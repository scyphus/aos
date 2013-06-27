#
# $Id$
#
# Copyright (c) 2013 Scyphus Solutions. All rights reserved.
# Authors:
#      Hirochika Asai  <asai@scyphus.co.jp>
#

all:
	@echo "make all is not supported."

fdtool:
	make -C tools/fdtool fdtool

tools: fdtool

fdimage:
	@echo "Make sure tools are compiled."
	make -C src diskboot
	make -C src loader
	make -C src kpack
	./tools/fdtool/fdtool ./aos.img src/diskboot src/loader src/kpack

image: fdimage

clean-image:
	make -C src clean

clean:
	make -C tools/fdtool clean
	make -C src clean
