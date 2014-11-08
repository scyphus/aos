/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#ifndef _AOS_CONST_H
#define _AOS_CONST_H

#define VERSION_MAJOR   0
#define VERSION_MINOR   0

#if __LP64__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

typedef u64 reg_t;

#if !defined(NULL)
#define NULL 0UL
#endif

#else
#error "Must be LP64"
#endif

#endif /* _AOS_CONST_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
