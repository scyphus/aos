#
# $Id$
#
# Copyright (c) 2013 Scyphus Solutions. All rights reserved.
# Authors:
#      Hirochika Asai  <asai@scyphus.co.jp>
#

all: fdtool

fdtool:
	make -C tools/fdtool fdtool

aos:
	make -C src

clean:
	make -C tools/fdtool clean
	make -C src clean
