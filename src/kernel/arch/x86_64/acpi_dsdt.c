/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

#include <aos/const.h>
#include "acpi.h"
#include "arch.h"

#define NAMESTRING_LEN  32

#define ZEROOP          0x00
#define ONEOP           0x01
#define ONESOP          0xff
#define NAMEOP          0x08
#define PACKAGEOP       0x12

#define BYTEPREFIX      0x0a
#define WORDPREFIX      0x0b
#define DWORDPREFIX     0x0c
#define STRINGPREFIX    0x0d
#define QWORDPREFIX     0x0e

#define SCOPEOP         0x10
#define BUFFEROP        0x11
#define PACKAGEOP       0x12
#define VARPACKAGEOP    0x13
#define METHODOP        0x14

#define EXTOPPREFIX     0x5b
#define  OPREGIONOP     0x80
#define  FIELDOP        0x81
#define  DEVICEOP       0x82
#define  INDEXFIELDOP   0x86

#define LOCAL0OP        0x60
#define LOCAL1OP        0x61
#define LOCAL2OP        0x62
#define LOCAL3OP        0x63
#define LOCAL4OP        0x64
#define LOCAL5OP        0x65
#define LOCAL6OP        0x66
#define LOCAL7OP        0x67

#define ARG0OP          0x68
#define ARG1OP          0x69
#define ARG2OP          0x6a
#define ARG3OP          0x6b
#define ARG4OP          0x6c
#define ARG5OP          0x6d
#define ARG6OP          0x6e
#define STOREOP         0x70

#define ADDOP           0x72
#define DECREMENTOP     0x76
#define SIZEOFOP        0x87

#define LANDOP          0x90
#define LNOTOP          0x92
#define LEQUALOP        0x93
#define LGREATEROP      0x94
#define LLESSOP         0x95

#define IFOP            0xa0
#define ELSEOP          0xa1
#define WHILEOP         0xa2
#define NOOPOP          0xa3
#define RETURNOP        0xa4
#define BREAKOP         0xa5



static int
_is_lead_name_char(u8 c)
{
    if ( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ) {
        return 1;
    } else {
        return 0;
    }
}

static int
_is_digit_char(u8 c)
{
    if ( c >= '0' && c <= '9' ) {
        return 1;
    } else {
        return 0;
    }
}

static int
_is_name_char(u8 c)
{
    if ( _is_lead_name_char(c) || _is_digit_char(c) ) {
        return 1;
    } else {
        return 0;
    }
}

static int
_is_root_char(u8 c)
{
    if ( '\\' == c ) {
        return 1;
    } else {
        return 0;
    }
}

static int
_name_string(u8 *d, int len, u8 *namestr)
{
    if ( len <= 0 ) {
        return -1;
    }
    /* FIXME: Must support string with other than 4 characters */
    if ( len < 4 ) {
        return -1;
    }
    if ( _is_lead_name_char(*d) || _is_root_char(*d) ) {
        namestr[0] = *d;
        if ( !_is_name_char(*(d + 1)) ) {
            namestr[1] = '\0';
            return 1;
        }
        namestr[1] = *(d + 1);
        if ( !_is_name_char(*(d + 2)) ) {
            namestr[2] = '\0';
            return 2;
        }
        namestr[2] = *(d + 2);
        if ( !_is_name_char(*(d + 2)) ) {
            namestr[3] = '\0';
            return 3;
        }
        namestr[3] = *(d + 3);
        namestr[4] = '\0';
        return 4;
    } else {
        kprintf("WARNING in NameString : %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\r\n",
                *d, *(d+1), *(d+2), *(d+3), *(d+4), *(d+5), *(d+6), *(d+7));
        namestr[0] = *d;
        namestr[1] = *(d + 1);
        namestr[2] = *(d + 2);
        namestr[3] = *(d + 3);
        namestr[4] = '\0';
        return 4;
    }
}

static int
_pkglength(u8 *d, int len, int *pkglen)
{
    int ret;

    if ( len <= 0 ) {
        return -1;
    }

    /* Get the following length */
    ret = (*d) >> 6;
    if ( len <= ret ) {
        return -1;
    }
    switch ( ret ) {
    case 0:
        *pkglen = (*d & 0x3f);
        break;
    case 1:
        *pkglen = (*d & 0xf) | ((int)*(d + 1) << 4);
        break;
    case 2:
        *pkglen = (*d & 0xf) | ((int)*(d + 1) << 4) | ((int)*(d + 2) << 12);
        break;
    case 3:
        *pkglen = (*d & 0xf) | ((int)*(d + 1) << 4) | ((int)*(d + 2) << 12)
            | ((int)*(d + 3) << 20);
        break;
    }

    return ret + 1;
}


struct label {
    u8 name[NAMESTRING_LEN];
    int type;
    u8 *pos;

    union {
        /* Name */
        struct {
            u8 *pos;
        } n;
        /* Field */
        struct {
            u8 acc;
            u8 space;
            int offset;
        } f;
        /* IndexField */
        struct {
            int idx;
            int dat;
            u8 flags;
            u8 space;
            int offset;
        } idxf;
    } u;
};
struct label_list {
    struct label l;
    struct label_list *prev;
    struct label_list *next;
};
struct region {
    u8 name[NAMESTRING_LEN];
    u8 space;
    int offset;
    int len;
};
struct region_list {
    struct region r;
    struct region_list *prev;
    struct region_list *next;
};
struct acpi_parser {
    struct label_list *labels;
    struct region_list *regions;
};


static int _dsdt(struct acpi_parser *, u8 *, int);
static int _eval_land(struct acpi_parser *, u8 *, int, int *);


static int
_add_label(struct acpi_parser *parser, u8 *namestr, u8 *pos)
{
    /* Store the position */
    struct label_list *ll;

    ll = kmalloc(sizeof(struct label_list));
    if ( NULL == ll ) {
        return -1;
    }
    kmemcpy(ll->l.name, namestr, NAMESTRING_LEN);
    ll->l.pos = pos;
    ll->l.type = 1;
    ll->prev = NULL;
    ll->next = NULL;
    if ( NULL == parser->labels ) {
        parser->labels = ll;
    } else {
        ll->next = parser->labels;
        parser->labels->prev = ll;
        parser->labels = ll;
    }

    return 0;
}

static int
_add_field(struct acpi_parser *parser, u8 *namestr, u8 acc, int space,
           int offset)
{
    /* Store the position */
    struct label_list *ll;

    ll = kmalloc(sizeof(struct label_list));
    if ( NULL == ll ) {
        return -1;
    }
    kmemcpy(ll->l.name, namestr, NAMESTRING_LEN);
    ll->l.type = 2;
    ll->l.u.f.acc = acc;
    ll->l.u.f.space = space;
    ll->l.u.f.offset = offset;
    ll->prev = NULL;
    ll->next = NULL;
    if ( NULL == parser->labels ) {
        parser->labels = ll;
    } else {
        ll->next = parser->labels;
        parser->labels->prev = ll;
        parser->labels = ll;
    }

    return 0;
}

static int
_find_field(struct acpi_parser *parser, u8 *namestr, u8 *acc, int *space,
             int *offset)
{
    struct label_list *ll;

    ll = parser->labels;
    while ( NULL != ll ) {
        if ( 0 == kstrcmp(ll->l.name, namestr) && ll->l.type == 2 ) {
            /* Found */
            *acc = ll->l.u.f.acc;
            *space = ll->l.u.f.space;
            *offset = ll->l.u.f.offset;
            return 0;
        }
        ll = ll->next;
    }

    /* Not found */
    return -1;

}

static int
_add_indexfield(struct acpi_parser *parser, u8 *namestr, int idx, int dat,
                u8 flags, u8 space, int offset)
{
    /* Store the position */
    struct label_list *ll;

    ll = kmalloc(sizeof(struct label_list));
    if ( NULL == ll ) {
        return -1;
    }
    kmemcpy(ll->l.name, namestr, NAMESTRING_LEN);
    ll->l.type = 3;
    ll->l.u.idxf.idx = idx;
    ll->l.u.idxf.dat = dat;
    ll->l.u.idxf.flags = flags;
    ll->l.u.idxf.space = space;
    ll->l.u.idxf.offset = offset;
    ll->prev = NULL;
    ll->next = NULL;
    if ( NULL == parser->labels ) {
        parser->labels = ll;
    } else {
        ll->next = parser->labels;
        parser->labels->prev = ll;
        parser->labels = ll;
    }

    return 0;
}


static int
_add_region(struct acpi_parser *parser, u8 *namestr, u8 space, int offset,
            int len)
{
    struct region_list *rl;

    rl = kmalloc(sizeof(struct region_list));
    if ( NULL == rl ) {
        return -1;
    }
    kmemcpy(rl->r.name, namestr, NAMESTRING_LEN);
    rl->r.space = space;
    rl->r.offset = offset;
    rl->r.len = len;
    rl->prev = NULL;
    rl->next = NULL;
    if ( NULL == parser->regions ) {
        parser->regions = rl;
    } else {
        rl->next = parser->regions;
        parser->regions->prev = rl;
        parser->regions = rl;
    }

    return 0;
}

static int
_find_region(struct acpi_parser *parser, u8 *namestr, u8 *space, int *offset,
             int *len)
{
    struct region_list *rl;

    rl = parser->regions;
    while ( NULL != rl ) {
        if ( 0 == kstrcmp(rl->r.name, namestr) ) {
            /* Found */
            *space = rl->r.space;
            *offset = rl->r.offset;
            *len = rl->r.len;
            return 0;
        }
        rl = rl->next;
    }

    /* Not found */
    return -1;
}



/*
 * Evaluate a name
 */
static int
_eval_name(struct acpi_parser *parser, struct label *l, int *val)
{
    u8 *d;

    d = l->pos;
    switch ( *d ) {
    case ZEROOP:
        *val = 0;
        break;
    case ONEOP:
        *val = 1;
        break;
    case ONESOP:
        *val = 0xff;
        break;
    default:
        kprintf("Cannot evaluate the name: %s %.2x %.2x %.2x %.2x\r\n",
                l->name, *d, *(d+1), *(d+2), *(d+3));
        return -1;
    }
    return 0;
}

/*
 * Evaluate an IndexField
 */
static int
_eval_indexfield(struct acpi_parser *parser, struct label *l, int *val)
{
    if ( l->u.idxf.space != 1 ) {
        /* Currently only support SystemIO */
        return -1;
    }

    outl(l->u.idxf.idx/8, l->u.idxf.offset/8);
    *val = inl(l->u.idxf.dat/8);

    return 0;
}

/*
 * Evaluate a label
 */
static int
_eval_label(struct acpi_parser *parser, u8 *namestr, int *val)
{
    struct label_list *ll;

    ll = parser->labels;
    while ( ll ) {
        if ( 0 == kstrcmp(ll->l.name, namestr) ) {
            /* Found */
            switch (ll->l.type) {
            case 1:
                /* Name */
                return _eval_name(parser, &ll->l, val);
                break;
            case 2:
                /* FIXME: Field */
                break;
            case 3:
                /* IndexField */
                return _eval_indexfield(parser, &ll->l, val);
                break;
            default:
                kprintf("Cannot evaluate the name: %s\r\n", namestr);
            }
            return 0;
        }
        ll = ll->next;
    }

    kprintf("WARNING: Unknown name: %s\r\n", namestr);
    *val = 0x00;

    /* FIXME */
    return 0;
}

/*
 * Evaluate an operand
 */
static int
_eval_operand(struct acpi_parser *parser, u8 *d, int len, int *val)
{
    int ret;
    u8 namestr[NAMESTRING_LEN];
    int ptr;

    if ( len <= 0 ) {
        return -1;
    }

    ptr = 0;

    /* Evaluate an operand */
    switch ( *d ) {
    case LANDOP:
        /* LandOp */
        return _eval_land(parser, d + 1, len - 1, val);
    default:
        /* NameString */
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
        ptr += ret;

        ret = _eval_label(parser, namestr, val);
        if ( ret < 0 ) {
            return -1;
        }
    }

    return ptr;
}

/*
 * Logical and
 * DefLAnd := LandOp Operand Operand
 */
static int
_eval_land(struct acpi_parser *parser, u8 *d, int len, int *val)
{
    int ret;
    int op0;
    int op1;
    int ptr;

    if ( len <= 0 ) {
        return -1;
    }

    ptr = 0;

    /* Operand */
    ret = _eval_operand(parser, d, len, &op0);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    /* Operand */
    ret = _eval_operand(parser, d, len, &op1);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    *val = op0 & op1;

    return ptr;
}

/*
 * Logical equal
 * DefLequal := LequalOp Operand Operand
 */
static
_eval_lequal(struct acpi_parser *parser, u8 *d, int len, int *val)
{
    int ret;
    int op0;
    int op1;
    int ptr;

    if ( len <= 0 ) {
        return -1;
    }

    ptr = 0;

    /* Operand */
    ret = _eval_operand(parser, d, len, &op0);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    /* Operand */
    ret = _eval_operand(parser, d, len, &op1);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    *val = (op0 == op1) ? 1 : 0;

    return ptr;
}

/*
 * Predicate := TermArg => Integer
 */
static int
_eval_predicate(struct acpi_parser *parser, u8 *d, int len, int *val)
{
    int ret;
    int ptr;

    if ( len <= 0 ) {
        return -1;
    }

    ptr = 0;

    switch ( *d ) {
    case LEQUALOP:
        ret = _eval_lequal(parser, d + 1, len - 1, val);
        if ( ret < 0 ) {
            return -1;
        }
        d += 1;
        len -= 1;
        ptr += 1;
        break;
    default:
        kprintf("Unknown predicate: %.2x %.2x %.2x %.2x\r\n",
                *d, *(d+1), *(d+2), *(d+3));
        return -1;
    }

    return ptr;
}

/*
 * DefIfElse := IfOp PkgLength Predicate TermList DefElse
 */
static int _eval(struct acpi_parser *, u8 *, int);
static int _eval_else_op(struct acpi_parser *, u8 *, int);
static int
_eval_if_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;
    int pkglen2;
    int val;
    int clen;
    u8 *e;
    int elen;

    /* PkgLength */
    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    clen = pkglen - ret;

    e = d + pkglen;
    elen = len - pkglen;

    /* Predicate */
    ret = _eval_predicate(parser, d, len, &val);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    clen -= ret;

    if ( val ) {
        /* If */
        ret = _eval(parser, d, clen);
        if ( ret < 0 ) {
            return -1;
        }
        /* Need to skip else */
        return pkglen;
    } else {
        /* Else */
        if ( ELSEOP == *d ) {
            pkglen2 = _eval_else_op(parser, e + 1, elen - 1);
            if ( pkglen2 < 0 ) {
                return -1;
            }
        } else {
            kprintf("Invalid DefElse\r\n");
        }
    }

    return pkglen;
}

static int
_eval_else_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;

    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }

    return pkglen;
}



static int
_eval(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int ptr;

    if ( len <= 0 ) {
        return -1;
    }
    ptr = 0;
    switch ( *d ) {
    case IFOP:
        ret = _eval_if_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case ELSEOP:
        kprintf("Invalid syntax: ELSEOP\r\n");
        /*
        ret = _eval_else_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        */
        return -1;
        break;

    }

    return ptr;
}


static int
_buffer_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;

    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }

    return pkglen;
}

static int
_name_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    u8 namestr[NAMESTRING_LEN];
    int ptr;

    ptr = 0;
    if ( len <= 0 ) {
        return -1;
    }

    /* NameString */
    ret = _name_string(d, len, namestr);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    /* FIXME: Store the position */
    if ( _add_label(parser, namestr, d) < 0 ) {
        return -1;
    }

    /* DataRefObject */
    if ( len <= 0 ) {
        return -1;
    }
    if ( 0 == *d ) {
        /* Zero */
        d += 1;
        len -= 1;
        ptr += 1;
    } else if ( PACKAGEOP == *d ) {
        /* FIXME */
        int pkglen;
        ret = _pkglength(d + 1, len - 1, &pkglen);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += pkglen + 1;
    } else if ( BUFFEROP == *d ) {
        /* ResourceTemplate */
        ret = _buffer_op(parser, d + 1, len - 1);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
    } else if ( DWORDPREFIX == *d ) {
        /* EISAID */
        d += 4 + 1;
        len -= 4 + 1;
        ptr += 4 + 1;
    } else {
        kprintf("**NAME** %s %x %x %x %x\r\n", namestr, *d, *(d+1), *(d+2), *(d+3));
        return -1;
    }

    return ptr;
}


static int
_region_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    u8 namestr[NAMESTRING_LEN];
    u8 rspace;
    int roffset;
    int rlen;
    int ptr;

    ptr = 0;
    if ( len <= 0 ) {
        return -1;
    }

    /* NameString */
    ret = _name_string(d, len, namestr);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    /* Region space */
    if ( len <= 0 ) {
        return -1;
    }
    rspace = *d;
    d++;
    len--;
    ptr++;

    /* Region offset */
    if ( len <= 0 ) {
        return -1;
    }
    switch ( *d ) {
    case BYTEPREFIX:
        roffset = *(u8 *)(d + 1);
        d += 2;
        len -= 2;
        ptr += 2;
        break;
    case WORDPREFIX:
        roffset = *(u16 *)(d + 1);
        d += 3;
        len -= 3;
        ptr += 3;
        break;
    default:
        return -1;
    }

    /* Region length */
    if ( len <= 0 ) {
        return -1;
    }
    switch ( *d ) {
    case BYTEPREFIX:
        rlen = *(u8 *)(d + 1);
        d += 2;
        len -= 2;
        ptr += 2;
        break;
    case WORDPREFIX:
        rlen = *(u16 *)(d + 1);
        d += 3;
        len -= 3;
        ptr += 3;
        break;
    default:
        return -1;
    }

#if 0
    if ( 0 == kstrcmp(namestr, "SYSI") ) {
        kprintf("Region: %x %x %x\r\n", rspace, roffset, rlen);
    }
#endif

    if ( _add_region(parser, namestr, rspace, roffset, rlen) < 0 ) {
        return -1;
    }

    return ptr;
}

static int
_field_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;
    u8 namestr[NAMESTRING_LEN];
    int ptr;
    u8 flags;
    int clen;

    ptr = 0;

    if ( len <= 0 ) {
        return -1;
    }

    /* PkgLength */
    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;
    clen = pkglen - ret;

    /* NameString */
    ret = _name_string(d, len, namestr);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;
    clen -= ret;

    /* FieldFlags */
    flags = *d;
    d += 1;
    len -= 1;
    ptr += 1;
    clen -= 1;

    /* Find the corresponding region */
    u8 rspace;
    int roffset;
    int rlen;
    ret = _find_region(parser, namestr, &rspace, &roffset, &rlen);
    if ( ret < 0 ) {
        /* Cannot find the corresponding region */
        return -1;
    }

    /* FieldList */
    int bitoff;
    bitoff = 0;
    while ( clen > 0 ) {
        if ( *d == 0 ) {
            /* FIXME: Offset */
            d += 1;
            clen -= 1;
            int offlen;
            ret = _pkglength(d, clen, &offlen);
            bitoff += offlen;
            d += ret;
            clen -= ret;
            continue;
        }
        /* NameString */
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        clen -= ret;

        ret = _add_field(parser, namestr, flags, rspace, roffset * 8 + bitoff);
        if ( ret < 0 ) {
            return -1;
        }

        /* Offset */
        bitoff += (u16)*d;
        d += 1;
        clen -= 1;
    }

#if 0
    if ( 0 == kstrcmp(namestr, "SYSI") ) {
        kprintf("SYSI:\r\n");
        kprintf("DEBUG: %.2x %.2x %.2x %.2x\r\n", *(d+0), *(d+1), *(d+2), *(d+3));
        kprintf("DEBUG: %.2x %.2x %.2x %.2x\r\n", *(d+4), *(d+5), *(d+6), *(d+7));

        int x0 = inl(0x4048);
        outl(0x4048, 16);
        int x1 = inl(0x4048 + 4);
        kprintf("**** %.8x %.8x\r\n", x0, x1);
    }
#endif

    return pkglen;
}

static int
_device_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;
    int ptr;
    u8 namestr[NAMESTRING_LEN];

    ptr = 0;

    /* PkgLength */
    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    /* NameString */
    ret = _name_string(d, len, namestr);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    /* ObjectList */
    len = pkglen - ret;
    ret = 0;
    while ( len > 0 ) {
        ret = _dsdt(parser, d, len);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
    }

    return pkglen;
}

/*
 * DefIndexField
 *      := IndexFieldOp PkgLength NameString NameString FieldFlags FieldList
 */
static int
_index_field_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;
    int clen;
    u8 namestr0[NAMESTRING_LEN];
    u8 namestr1[NAMESTRING_LEN];
    int flags;

    if ( len <= 0 ) {
        return -1;
    }

    /* PkgLength */
    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    clen = pkglen - ret;

    /* NameString */
    ret = _name_string(d, len, namestr0);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    clen -= ret;

    /* NameString */
    ret = _name_string(d, len, namestr1);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    clen -= ret;

    /* FieldFlags */
    flags = *d;
    d += 1;
    len -= 1;
    clen -= 1;

    if ( flags & 0xf ) {
    }

    int idx;
    int dat;
    u8 acc;
    int space;
    /* Find field */
    ret = _find_field(parser, namestr0, &acc, &space, &idx);
    if ( ret < 0 ) {
        return -1;
    }

    /* Find field */
    ret = _find_field(parser, namestr1, &acc, &space, &dat);
    if ( ret < 0 ) {
        return -1;
    }

    /* FieldList */
    int bitoff;
    u8 namestr[NAMESTRING_LEN];
    bitoff = 0;
    while ( clen > 0 ) {
        if ( *d == 0 ) {
            /* FIXME: Offset */
            d += 1;
            clen -= 1;
            int offlen;
            ret = _pkglength(d, clen, &offlen);
            bitoff += offlen;
            d += ret;
            clen -= ret;
            continue;
        }
        /* NameString */
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        clen -= ret;

        ret = _add_indexfield(parser, namestr, idx, dat, flags, space, bitoff);
        if ( ret < 0 ) {
            return -1;
        }

        /* Offset */
        bitoff += (u16)*d;
        d += 1;
        clen -= 1;
    }

    /* FieldList */
    //kprintf("DEBUG: %.2x %.2x %.2x %.2x\r\n", *(d+4), *(d+5), *(d+6), *(d+7));

    return pkglen;
}

static int
_scope_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;
    int ptr;
    u8 namestr[NAMESTRING_LEN];

    ptr = 0;

    /* PkgLength */
    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    /* NameString */
    ret = _name_string(d, len, namestr);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;

    /* TermList */
    len = pkglen - ret;
    ret = 0;
    while ( len > 0 ) {
        ret = _dsdt(parser, d, len);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
    }

    return pkglen;
}

static int
_method_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;
    int ptr;
    u8 namestr[NAMESTRING_LEN];
    u8 flags;
    int clen;

    ptr = 0;

    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;
    clen = pkglen - ret;

    /* NameString */
    ret = _name_string(d, len, namestr);
    if ( ret < 0 ) {
        return -1;
    }
    d += ret;
    len -= ret;
    ptr += ret;
    clen -= ret;

    /* MethodFlags */
    flags = *d;
    d += 1;
    len -= 1;
    ptr -= 1;
    clen -= 1;

    /* TermList */
    if ( 0 == kstrcmp(namestr, "_PRT") ) {
        /* Evaluate _PRT method */
        //kprintf("Evaluate method: %s\r\n", namestr);
        len = clen;
        ret = 0;
        while ( len > 0 ) {
            ret = _eval(parser, d, len);
            if ( ret < 0 ) {
                return -1;
            }
            d += ret;
            len -= ret;
        }
    }

    return pkglen;
}

static int
_store_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    u8 namestr[NAMESTRING_LEN];
    int ptr;

    ptr = 0;

    /* TermArg */
    switch ( *d ) {
    case ARG0OP:
    case ARG1OP:
    case ARG2OP:
    case ARG3OP:
    case ARG4OP:
    case ARG5OP:
    case ARG6OP:
    case ZEROOP:
        d += 1;
        len -= 1;
        ptr += 1;
        break;
    case SIZEOFOP:
        d += 1;
        len -= 1;
        ptr += 1;
        switch ( *d ) {
        case LOCAL0OP:
        case LOCAL1OP:
        case LOCAL2OP:
        case LOCAL3OP:
        case LOCAL4OP:
        case LOCAL5OP:
        case LOCAL6OP:
        case LOCAL7OP:
            d += 1;
            len -= 1;
            ptr += 1;
            break;
        default:
            kprintf("Unknown TermArg: %.2x %.2x %.2x %.2x\r\n",
                    *d, *(d+1), *(d+2), *(d+3));
            return -1;
        }
        break;
#if 0
    case WORDPREFIX:
        d += 2 + 1;
        len -= 4 + 1;
        ptr += 4 + 1;
        break;
    case DWORDPREFIX:
        d += 4 + 1;
        len -= 4 + 1;
        ptr += 4 + 1;
        break;
    case QWORDPREFIX:
        d += 8 + 1;
        len -= 4 + 1;
        ptr += 4 + 1;
        break;
#endif
    default:
        /* Method / Name */
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            kprintf("Unknown TermArg: %.2x %.2x %.2x %.2x\r\n",
                    *d, *(d+1), *(d+2), *(d+3));
            return -1;
        }
        d += ret;
        len -= ret;
        ptr += ret;
        switch ( *d ) {
        case LOCAL0OP:
        case LOCAL1OP:
        case LOCAL2OP:
        case LOCAL3OP:
        case LOCAL4OP:
        case LOCAL5OP:
        case LOCAL6OP:
        case LOCAL7OP:
        case ONEOP:
        case ONESOP:
            d += 1;
            len -= 1;
            ptr += 1;
            break;
        default:
            kprintf("Unknown TermArg: %.2x %.2x %.2x %.2x\r\n",
                    *d, *(d+1), *(d+2), *(d+3));
            return -1;
        }
    }

    /* SuperName */
    switch ( *d ) {
    case LOCAL0OP:
    case LOCAL1OP:
    case LOCAL2OP:
    case LOCAL3OP:
    case LOCAL4OP:
    case LOCAL5OP:
    case LOCAL6OP:
    case LOCAL7OP:
        d += 1;
        len -= 1;
        ptr += 1;
        break;
    default:
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
        ptr += ret;
    }

    return ptr;
}

static int
_add_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    u8 namestr[NAMESTRING_LEN];
    int ptr;

    ptr = 0;

    /* Operand */
    switch ( *d ) {
    case LOCAL0OP:
    case LOCAL1OP:
    case LOCAL2OP:
    case LOCAL3OP:
    case LOCAL4OP:
    case LOCAL5OP:
    case LOCAL6OP:
    case LOCAL7OP:
    case ONEOP:
    case ONESOP:
        d += 1;
        len -= 1;
        ptr += 1;
        break;
    default:
        /* Method / Name */
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
        ptr += ret;
        switch ( *d ) {
        case LOCAL0OP:
        case LOCAL1OP:
        case LOCAL2OP:
        case LOCAL3OP:
        case LOCAL4OP:
        case LOCAL5OP:
        case LOCAL6OP:
        case LOCAL7OP:
        case ONEOP:
        case ONESOP:
            d += 1;
            len -= 1;
            ptr += 1;
            break;
        default:
            return -1;
        }
    }

    /* Operand */
    switch ( *d ) {
    case LOCAL0OP:
    case LOCAL1OP:
    case LOCAL2OP:
    case LOCAL3OP:
    case LOCAL4OP:
    case LOCAL5OP:
    case LOCAL6OP:
    case LOCAL7OP:
    case ONEOP:
    case ONESOP:
        d += 1;
        len -= 1;
        ptr += 1;
        break;
    default:
        /* Method / Name */
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
        ptr += ret;
    }

    /* Target */
    switch ( *d ) {
    case LOCAL0OP:
    case LOCAL1OP:
    case LOCAL2OP:
    case LOCAL3OP:
    case LOCAL4OP:
    case LOCAL5OP:
    case LOCAL6OP:
    case LOCAL7OP:
        d += 1;
        len -= 1;
        ptr += 1;
        break;
    default:
        /* Method / Name */
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
        ptr += ret;
    }

    return ptr;
}

static int
_decrement_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    u8 namestr[NAMESTRING_LEN];
    int ptr;

    ptr = 0;

    /* SuperName */
    switch ( *d ) {
    case LOCAL0OP:
    case LOCAL1OP:
    case LOCAL2OP:
    case LOCAL3OP:
    case LOCAL4OP:
    case LOCAL5OP:
    case LOCAL6OP:
    case LOCAL7OP:
        d += 1;
        len -= 1;
        ptr += 1;
        break;
    default:
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
        ptr += ret;
    }

    return ptr;
}

static int
_if_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;

    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }

    return pkglen;
}

static int
_else_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;

    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }

    return pkglen;
}

static int
_while_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int pkglen;

    ret = _pkglength(d, len, &pkglen);
    if ( ret < 0 ) {
        return -1;
    }

    return pkglen;
}

static int
_return_op(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int ptr;
    u8 namestr[NAMESTRING_LEN];

    ptr = 0;

    /* ArgObject */
    if ( *d == SIZEOFOP ) {
        d += 1;
        len -= 1;
        ptr += 1;
        /* SuperName */
        switch ( *d ) {
        case LOCAL0OP:
        case LOCAL1OP:
        case LOCAL2OP:
        case LOCAL3OP:
        case LOCAL4OP:
        case LOCAL5OP:
        case LOCAL6OP:
        case LOCAL7OP:
            d += 1;
            len -= 1;
            ptr += 1;
            break;
        default:
            ret = _name_string(d, len, namestr);
            if ( ret < 0 ) {
                return -1;
            }
            d += ret;
            len -= ret;
            ptr += ret;
        }
    } else {
        /* NameString */
        ret = _name_string(d, len, namestr);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
        ptr += ret;
    }

    return ptr;
}

static int
_ext_op_prefix(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;

    if ( len <= 0 ) {
        return -1;
    }

    switch ( *d ) {
    case OPREGIONOP:
        ret = _region_op(parser, d + 1, len - 1);
        break;
    case FIELDOP:
        ret = _field_op(parser, d + 1, len - 1);
        break;
    case DEVICEOP:
        ret = _device_op(parser, d + 1, len - 1);
        break;
    case INDEXFIELDOP:
        ret = _index_field_op(parser, d + 1, len - 1);
        break;
    default:
        kprintf("Unknown (ext): %x\r\n", *d);
        int i;
        for ( i = 0; i < 32; i++ ) {
            kprintf("%.2x ", *(d+i));
            if ( 15 == i % 16 ) {
                kprintf("\r\n");
            }
        }
        return -1;
    }
    if ( ret < 0 ) {
        return -1;
    }

    return ret + 1;
}

static int
_dsdt(struct acpi_parser *parser, u8 *d, int len)
{
    int ret;
    int ptr;

    if ( len <= 0 ) {
        return -1;
    }
    ptr = 0;
    switch ( *d ) {
    case NAMEOP:
        ret = _name_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            kprintf("**** %x %x %x %x\r\n", *(d+5), *(d+6), *(d+7), *(d+8));
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case SCOPEOP:
        ret = _scope_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case METHODOP:
        ret = _method_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case EXTOPPREFIX:
        /* ExtOppPefix */
        ret = _ext_op_prefix(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case STOREOP:
        ret = _store_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case ADDOP:
        ret = _add_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case DECREMENTOP:
        ret = _decrement_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case IFOP:
        ret = _if_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case ELSEOP:
        ret = _else_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case WHILEOP:
        ret = _while_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case RETURNOP:
        ret = _return_op(parser, d + 1, len - 1);
        if ( ret <= 0 ) {
            return -1;
        }
        d += ret + 1;
        len -= ret + 1;
        ptr += ret + 1;
        break;
    case ZEROOP:
        d += 1;
        len -= 1;
        ptr += 1;
        break;
    default:
        kprintf("Unknown: %x %x\r\n", *d, len);
        int i;
        for ( i = 0; i < 32; i++ ) {
            kprintf("%.2x ", *(d+i));
            if ( 15 == i % 16 ) {
                kprintf("\r\n");
            }
        }
        return -1;
    }

    return ptr;
}

int
acpi_parse_dsdt_root(u8 *d, int len)
{
    int ret;
    struct acpi_parser parser;

    parser.labels = NULL;
    parser.regions = NULL;

    if ( len < 0 ) {
        return -1;
    }

    ret = 0;
    while ( len > 0 ) {
        ret = _dsdt(&parser, d, len);
        if ( ret < 0 ) {
            return -1;
        }
        d += ret;
        len -= ret;
    }

    return 0;

#if 0
    int i;
#if 0
    for ( i = 0; i < 128; i++ ) {
        kprintf("%.2x ", *(d+i));
        if ( 15 == i % 16 ) {
            kprintf("\r\n");
        }
    }
    kprintf("\r\n=====\r\n");
#endif
    kprintf("%x\r\n", len);

    while ( len >= 5 ) {
        if ( 0 == cmp(d, (u8 *)"_PRT", 4) ) {
            //d-= 8;
            kprintf("XXXXXX*****XXXX\r\n");
#if 0
            for ( i = 64; i < 128; i++ ) {
                kprintf("%.2x ", *(d+i));
                if ( 15 == i % 16 ) {
                    kprintf("\r\n");
                }
            }
#endif
            //break;
        }
        d++;
        len--;
    }


    //len = 0x20000;
    //d = (u8 *)0xe0000;
    len = 0x1024;
    d = (u8 *)0x9fc00;
    while ( len >= 5 ) {
        if ( 0 == cmp(d, (u8 *)"_MP_", 4) ) {
            //d-= 8;
            kprintf("YYYY*****XXXX %llx\r\n", d);
            d = (u8 *)0x0e1300;
            for ( i = 0 + 128 + 128; i < 64 + 128 + 128; i++ ) {
                kprintf("%.2x ", *(d+i));
                if ( 15 == i % 16 ) {
                    kprintf("\r\n");
                }
            }
            break;
        }
        d+=1;
        len-=1;
    }

    return 0;
#endif
}






/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
