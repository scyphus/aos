/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "kernel.h"

struct processor_table *processors;

int this_cpu(void);

/*
 * Initialize the processor table
 */
int
processor_init(void)
{
    int i;
    int npr;

    /* Count the number of processors */
    npr = 0;
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        if ( arch_cpu_active(i) ) {
            npr++;
        }
    }
    processors = kmalloc(sizeof(struct processor_table));
    processors->n = npr;
    processors->prs = kmalloc(sizeof(struct processor) * npr);
    npr = 0;
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        if ( arch_cpu_active(i) ) {
            if ( processors->n <= npr ) {
                /* Error */
                return -1;
            }
            processors->prs[npr].id = i;
            if ( i == 0 ) {
                processors->prs[npr].type = PROCESSOR_BSP;
            } else {
                processors->prs[npr].type = PROCESSOR_AP_TICKLESS;
            }
            processors->map[i] = npr;

            npr++;
        }
    }

    return 0;
}

/*
 * Get this processor structure
 */
struct processor *
processor_this(void)
{
    return &processors->prs[processors->map[this_cpu()]];
}

/*
 * Get the specified processor structure
 */
struct processor *
processor_get(u8 id)
{
    return &processors->prs[processors->map[id]];
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
