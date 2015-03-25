/*_
 * Copyright (c) 2014-2015 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include "kernel.h"

struct sail *
sail_init(void)
{
    struct sail *sail;
    int i;
    u16 *bcn16;

    sail = kmalloc(sizeof(struct sail));

    bcn16 = kmalloc(sizeof(u16) * (1 << 16));
    if ( NULL == bcn16 ) {
        return NULL;
    }
    for ( i = 0; i < (1 << 16); i++ ) {
        bcn16[i] = 1;
        arch_busy_usleep(1);
    }

    sail->radix = NULL;
    sail->bcn16 = bcn16;
    sail->bn24 = NULL;
    sail->c24 = NULL;
    sail->n32 = NULL;

    sail->fib.sz = 4096;
    sail->fib.n = 0;
    sail->fib.entries = kmalloc(sizeof(u64) * sail->fib.sz);
    sail->fib.entries[sail->fib.n++] = NH_NOENTRY;

    return sail;
}

static int
_rt_route_add(struct sail *sail, struct radix_node **node,
              struct radix_node *parent, u32 prefix, int len, u32 nexthop,
              int depth)
{
    if ( NULL == *node ) {
        *node = kmalloc(sizeof(struct radix_node));
        if ( NULL == *node ) {
            /* Memory error */
            return -1;
        }
        (*node)->valid = 0;
        (*node)->parent = parent;
        (*node)->left = NULL;
        (*node)->right = NULL;
        //(*node)->prefix = prefix & ~(((u64)1 << (32 - depth)) - 1);
        (*node)->len = depth;
    }

    if ( len == depth ) {
        /* Matched */
        if ( (*node)->valid ) {
            /* Already exists */
            return -1;
        }
        (*node)->valid = 1;
        (*node)->nexthop = nexthop;
        (*node)->len = len;

        return 0;
    } else {
        if ( (prefix >> (32 - depth - 1)) & 1 ) {
            /* Right */
            return _rt_route_add(sail, &((*node)->right), *node, prefix, len,
                                 nexthop, depth + 1);
        } else {
            /* Left */
            return _rt_route_add(sail, &((*node)->left), *node, prefix, len,
                                 nexthop, depth + 1);
        }
    }
}
int
sail_route_add(struct sail *sail, u32 prefix, int len, u32 nexthop)
{
    int ret;
    int i;
    int n;

    n = 0;
    for ( i = 0; i < sail->fib.n; i++ ) {
        if ( sail->fib.entries[i] == nexthop ) {
            n = i;
            break;
        }
    }
    if ( i == sail->fib.n ) {
        n = sail->fib.n;
        sail->fib.entries[n] = nexthop;
        sail->fib.n++;
    }

    /* Insert to the radix tree */
    ret = _rt_route_add(sail, &sail->radix, NULL, prefix, len, n, 0);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

void
_rebuild_mark(struct radix_node *node, u8 *b16, u8 *b24, u64 pr, int depth)
{
    if ( NULL == node ) {
        return;
    }

    if ( 16 == depth ) {
        if ( node->left || node->right ) {
            b16[pr >> 3] |= 1 << (pr & 0x7);
        }
    } else if ( 24 == depth ) {
        if ( node->left || node->right ) {
            b24[pr >> 3] |= 1 << (pr & 0x7);
        }
    }

    /* Prefix */
    pr <<= 1;

    /* Left */
    _rebuild_mark(node->left, b16, b24, pr, depth + 1);
    /* Right */
    _rebuild_mark(node->right, b16, b24, pr | 1, depth + 1);
}

static u32
_getnh(struct radix_node *node, u32 prefix, int depth, int len,
       struct radix_node *en)
{
    if ( NULL == node ) {
        return NH_NOENTRY;
    }
    if ( node->valid ) {
        en = node;
    }
    if ( depth >= len ) {
        if ( NULL != en ) {
            return en->nexthop;
        } else {
            return NH_NOENTRY;
        }
    }

    if ( (prefix >> (32 - depth - 1)) & 1 ) {
        /* Right */
        if ( NULL == node->right ) {
            if ( NULL != en ) {
                return en->nexthop;
            } else {
                return NH_NOENTRY;
            }
        } else {
            return _getnh(node->right, prefix, depth + 1, len, en);
        }
    } else {
        /* Left */
        if ( NULL == node->left ) {
            if ( NULL != en ) {
                return en->nexthop;
            } else {
                return NH_NOENTRY;
            }
        } else {
            return _getnh(node->left, prefix, depth + 1, len, en);
        }
    }
}

int
sail_commit(struct sail *sail)
{
    u8 *b[25];
    u16 *n[25];
    int i;

    for ( i = 0; i < 25; i++ ) {
        b[i] = kmalloc(((1<<i) + 7) / 8);
        kmemset(b[i], 0, ((1<<i) + 7) / 8);
        n[i] = kmalloc(sizeof(u16) * (1<<i));
        kmemset(n[i], 0, sizeof(u16) * (1<<i));
    }

    u8 *b16 = kmalloc(((1<<16) + 7) / 8);
    kmemset(b16, 0, ((1<<16) + 7) / 8);
    u8 *b24 = kmalloc(((1<<24) + 7) / 8);
    kmemset(b24, 0, ((1<<24) + 7) / 8);

    /* Mark b16 and b24, which have children */
    _rebuild_mark(sail->radix, b16, b24, 0, 0);

    int bit;
    int cnt24 = 0;
    int cnt32 = 0;
    for ( i = 0; i < (1 << 16); i++ ) {
        bit = *(b16 + (i >> 3)) & (1 << (i & 0x7));
        if ( bit ) {
            cnt24++;
        }
    }
    for ( i = 0; i < (1 << 24); i++ ) {
        bit = *(b24 + (i >> 3)) & (1 << (i & 0x7));
        if ( bit ) {
            cnt32++;
        }
    }

    u16 *bcn16 = kmalloc(sizeof(u16) * (1<<16));
    u16 *bn24 = kmalloc(sizeof(u16) * 256 * cnt24);
    u16 *c24 = kmalloc(sizeof(u16) * (1<<24));
    u16 *n32 = kmalloc(sizeof(u16) * 256 * cnt32);
    u16 nh;

    int a;
    int j;

    /* /16 */
    a = 0;
    for ( i = 0; i < (1 << 16); i++ ) {
        bit = *(b16 + (i >> 3)) & (1 << (i & 0x7));
        if ( bit ) {
            /* C<<1 */
            bcn16[i] = 0 | ((a + 1) << 1);
            a++;
        } else {
            nh = _getnh(sail->radix, (u32)i << 16, 0, 16, NULL);
            /* N<<1 */
            bcn16[i] = 1 | ((nh + 1) << 1);
        }
    }

    u32 tmp;
    /* /24 */
    a = 0;
    for ( i = 0; i < (1 << 24); i++ ) {
        bit = *(b24 + (i >> 3)) & (1 << (i & 0x7));
        if ( bit ) {
            /* BN24 = B24 = 0 */
            tmp = bcn16[i >> 8] >> 1;
            tmp = (tmp - 1) << 8;
            bn24[tmp + (i & 0xff)] = 0;
            c24[i] = a + 1;
            if ( 0 != (bcn16[i >> 8] & 1) ) {
                return -1;
            }
            for ( j = 0; j < 256; j++ ) {
                nh = _getnh(sail->radix, ((u32)i << 8) + j, 0, 32, NULL);
                n32[(a << 8) + j] = nh + 1;
            }
            a++;
        } else {
            /* BN24 = N24 */
            if ( 0 == (bcn16[i >> 8] & 1) ) {
                tmp = bcn16[i >> 8] >> 1;
                tmp = (tmp - 1) << 8;
                nh = _getnh(sail->radix, (u32)i << 8, 0, 24, NULL);
                bn24[tmp + (i & 0xff)] = nh + 1;
            }
        }
    }

    for ( i = 0; i < 25; i++ ) {
        kfree(b[i]);
        kfree(n[i]);
    }
    kfree(b16);
    kfree(b24);

    if ( sail->bcn16 ) {
        kfree(sail->bcn16);
    }
    if ( sail->bn24 ) {
        kfree(sail->bn24);
    }
    if ( sail->c24 ) {
        kfree(sail->c24);
    }
    if ( sail->n32 ) {
        kfree(sail->n32);
    }

    sail->bcn16 = bcn16;
    sail->bn24 = bn24;
    sail->c24 = c24;
    sail->n32 = n32;

    return 0;
}

u64
sail_lookup(struct sail *sail, u32 addr)
{
    u16 c16;
    int fidx;

    if ( sail->bcn16[addr >> 16] & 1 ) {
        /* N = sail->bcn16[addr >> 16] >> 1  */
        fidx = (sail->bcn16[addr >> 16] >> 1);
        return sail->fib.entries[fidx - 1];
    }
    c16 = (sail->bcn16[addr >> 16] >> 1) - 1;
    if ( sail->bn24[((u32)c16 << 8) + ((addr >> 8) & 0xff)] ) {
        /* N = sail->bn24[c16 + ((addr >> 8) & 0xff)] */
        fidx = sail->bn24[((u32)c16 << 8) + ((addr >> 8) & 0xff)];
        return sail->fib.entries[fidx - 1];
    }
    fidx = sail->n32[((u32)(sail->c24[addr >> 8] - 1) << 8) + (addr & 0xff)];
    return sail->fib.entries[fidx - 1];
}



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
