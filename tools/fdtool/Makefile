#
# $Id$
#
# Copyright (c) 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
# Authors:
#      Hirochika Asai  <asai@scyphus.co.jp>
#

fdtool: fdtool.o
	$(CC) -o $@ $^

.c.o:
	$(CC) -c -o $@ $<

.SUFFIX: .c .o

clean:
	-rm -f *.o
	-rm -f fdtool
