/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014-2015 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include "kernel.h"

#define DXR_X   18


/*
 * Initialize DXR structure
 */
struct dxr *
dxr_init(void)
{
    struct dxr *dxr;

    dxr = kmalloc(sizeof(struct dxr));
    if  ( NULL == dxr ) {
        return NULL;
    }
    dxr->range.head = NULL;
    dxr->range.tail = NULL;
    dxr->nhs = NULL;

    dxr->lut = NULL;

    dxr->radix = NULL;

    return dxr;
}


/*
 * Add a route
 */
static int
_rt_route_add(struct dxr *dxr, struct radix_node **node,
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
            return _rt_route_add(dxr, &((*node)->right), *node, prefix, len,
                                 nexthop, depth + 1);
        } else {
            /* Left */
            return _rt_route_add(dxr, &((*node)->left), *node, prefix, len,
                                 nexthop, depth + 1);
        }
    }
}
int
dxr_route_add(struct dxr *dxr, u32 prefix, int len, u32 nexthop)
{
    int ret;

    /* Insert to the radix tree */
    ret = _rt_route_add(dxr, &dxr->radix, NULL, prefix, len, nexthop, 0);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}



static int
_add_range(struct dxr *dxr, u32 begin, u32 end, u64 nexthop)
{
    struct dxr_range *r;
    struct dxr_next_hop **nh;
    u32 chunk;
    u32 mask;
    u64 b;

    /* Insert next hop first */
    nh = &(dxr->nhs);

    while ( *nh ) {
        if ( (*nh)->addr == nexthop ) {
            break;
        }
        nh = &((*nh)->next);
    }
    if ( NULL == *nh ) {
        *nh = kmalloc(sizeof(struct dxr_next_hop));
        if ( NULL == *nh ) {
            return -1;
        }
        (*nh)->addr = nexthop;
        (*nh)->next = NULL;
    }

    chunk = (1 << (32 - DXR_X));
    mask = ~(chunk - 1);

    b = begin;
    for ( ; b <= end; b = (b & mask) + chunk ) {
        r = kmalloc(sizeof(struct dxr_range));
        if ( NULL == r ) {
            /* FIXME */
            return -1;
        }
        r->begin = b;
        if ( (end & mask) == (b & mask) ) {
            r->end = end;
        } else {
            r->end = (b & mask) + chunk - 1;
        }
        r->nh = *nh;
        r->next = NULL;

        if ( dxr->range.tail ) {
            dxr->range.tail->next = r;
            dxr->range.tail = r;
        } else {
            dxr->range.head = r;
            dxr->range.tail = r;
        }
    }

    return 0;
}

static void
_compile_range(struct dxr *dxr, struct radix_node *node, u64 prefix, int depth,
               long long int *pos, u32 *nexthop, struct radix_node *en)
{
    u32 nnh;
    u32 start;

    if ( NULL == node ) {
        start = ((u64)prefix << (64 - depth) >> 32);
        //end = start + ((u64)1 << (32 - depth)) - 1;

        if ( NULL != en ) {
            nnh = en->nexthop;
        } else {
            nnh = NH_NOENTRY;
        }
        if ( nnh != *nexthop ) {
            /* Different nexthop */
            if ( *pos >= 0 ) {
                _add_range(dxr, *pos, start - 1, *nexthop);
            } else if ( start > 0 ) {
                _add_range(dxr, 0, start - 1, NH_NOENTRY);
            }

            *nexthop = nnh;
            *pos = ((u64)prefix << (64 - depth) >> 32);
        }
        return;
    }

    if ( node->valid ) {
        en = node;
    }

    _compile_range(dxr, node->left, prefix << 1, depth + 1, pos, nexthop, en);

    _compile_range(dxr, node->right, (prefix << 1) | 1, depth + 1, pos,
                   nexthop, en);
}
int
dxr_commit(struct dxr *dxr)
{
    int i;
    struct dxr_range *r;
    struct dxr_next_hop *nh;
    struct dxr_lookup_table_entry *ltes;
    int ridx;
    int full;
    u32 v;

    dxr->range.head = NULL;
    dxr->range.tail = NULL;
    long long int pos = -1;
    u32 nexthop = NH_NOENTRY;
    _compile_range(dxr, dxr->radix, 0, 0, &pos, &nexthop, NULL);
    if ( pos >= 0 ) {
        _add_range(dxr, pos, ((u64)1 << 32) - 1, nexthop);
    }

    if ( NULL != dxr->lut ) {
        kfree(dxr->lut);
    }

    /* Indexing */
    nh = dxr->nhs;
    i = 0;
    while ( nh ) {
        nh->idx = i;
        i++;
        nh = nh->next;
    }

    /* Range */
    dxr->nh = kmalloc(i * sizeof(u64));
    if ( NULL == dxr->nh ) {
        return -1;
    }
    i = 0;
    nh = dxr->nhs;
    while ( nh ) {
        dxr->nh[i++] = nh->addr;
        nh = nh->next;
    }


    ltes = kmalloc(sizeof(struct dxr_lookup_table_entry) * (1 << DXR_X));
    for ( i = 0; i < (1 << DXR_X); i++ ) {
        ltes[i].nr = 0;
        /* Prevent short format */
        ltes[i].stype = -1;
        arch_busy_usleep(1);
    }
    r = dxr->range.head;
    while ( NULL != r ) {
        i = (r->begin >> (32 - DXR_X));
        if ( 0 == (r->begin & 0xff) && 0xff == (r->end & 0xff) ) {
            ltes[i].stype++;
        }
        ltes[i].nr++;
        r = r->next;
    }

    ridx = 0;
    full = 0;
    for ( i = 0; i < (1 << DXR_X); i++ ) {
        if ( ltes[i].nr <= 1 ) {
            /* Direct */
        } else {
            /* Long: assuming D16R or D18R */
            ridx += (4 * ltes[i].nr);
            full++;
        }
    }

    /* Range */
    dxr->rt = kmalloc(ridx);
    if ( NULL == dxr->rt ) {
        kfree(dxr->nh);
        kfree(ltes);
        return -1;
    }

    /* Lookup table */
    dxr->lut = kmalloc(4 * (1 << DXR_X));
    if ( NULL == dxr->lut ) {
        kfree(dxr->rt);
        kfree(dxr->nh);
        kfree(ltes);
        return -1;
    }
    ridx = 0;
    for ( i = 0; i < (1 << DXR_X); i++ ) {
        if ( ltes[i].nr <= 1 ) {
            /* Direct */
            dxr->lut[i] = 0;
        } else {
            /* Long: assuming D16R or D18R */
            dxr->lut[i] = (ltes[i].nr << 20) | (ridx / 4);
            ridx += (4 * ltes[i].nr);
        }
    }

    /* Range */
    r = dxr->range.head;
    ridx = 0;
    while ( NULL != r ) {
        i = (r->begin >> (32 - DXR_X));
        v = r->begin & ((1 << (32 - DXR_X)) - 1);
        if ( ltes[i].nr <= 1 ) {
            /* Direct */
            dxr->lut[i] = r->nh->idx;
        } else {
            /* Long: little endian */
            dxr->rt[ridx++] = v & 0xff;
            dxr->rt[ridx++] = (v >> 8) & 0xff;
            dxr->rt[ridx++] = r->nh->idx & 0xff;
            dxr->rt[ridx++] = (r->nh->idx >> 8) & 0xff;
        }
        r = r->next;
    }

    kfree(ltes);

    return 0;
}

/*
 * Lookup
 */
u64
dxr_lookup(struct dxr *dxr, u32 addr)
{
    int idx;
    int nr;
    int ridx;
    int i;
    int bl;
    int bh;
    u32 b;

    idx = (addr >> (32 - DXR_X));

    if ( 0 == (dxr->lut[idx] >> 20) ) {
        /* Direct */
        return dxr->nh[dxr->lut[idx] & ((1 << 20) - 1)];
    } else {
        /* Binary search */
        nr = (dxr->lut[idx] >> 20);
        ridx = (dxr->lut[idx] & ((1 << 20) - 1));
        bl = 0;
        bh = nr;
        b = addr & ((1 << (32 - DXR_X)) - 1);
        for ( ;; ) {
            i = (bh - bl) / 2 + bl;
            if ( b >= (u16)*(u16 *)(dxr->rt + (ridx + i) * 4)
                 && (i == nr - 1
                     || b < (u16)*(u16 *)(dxr->rt + (ridx + i + 1) * 4)) ) {
                /* Match */
                return dxr->nh[(u16)*(u16 *)(dxr->rt + (ridx + i) * 4 + 2)];
            } else if ( b <= (u16)*(u16 *)(dxr->rt + (ridx + i) * 4) ) {
                bh = i;
            } else {
                bl = i + 1;
            }
        }
        /* Not to reached here */
        return 0;
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
