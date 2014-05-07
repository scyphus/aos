/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* Long jump */
void ljmp64(void *ptr);

/*
 * Boot function
 */
void
cboot(void)
{
    unsigned short *x = (unsigned short *)0x000b8000;
    //*x = (0x07 << 8) | 'x';

    /* Long jump */
    ljmp64(0x10000);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
