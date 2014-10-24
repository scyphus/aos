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
        kprintf("WARNING in NameString : %.2x %.2x %.2x %.2x\r\n",
                *d, *(d+1), *(d+2), *(d+3));
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
    u8 *pos;
    int type;
};
struct label_list {
    struct label l;
    struct label_list *prev;
    struct label_list *next;
};
struct acpi_parser {
    struct label_list *labels;
};

static int _dsdt(struct acpi_parser *, u8 *, int);



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
        d += 2;
        len -= 2;
        ptr += 2;
        break;
    case WORDPREFIX:
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
        d += 2;
        len -= 2;
        ptr += 2;
        break;
    case WORDPREFIX:
        d += 3;
        len -= 3;
        ptr += 3;
        break;
    default:
        return -1;
    }

    return ptr;
}

static int
_field_op(struct acpi_parser *parser, u8 *d, int len)
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

static int
_index_field_op(struct acpi_parser *parser, u8 *d, int len)
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

    kprintf("Method: %s\r\n", namestr);
    /* TermList */
    if ( 0 == kstrcmp(namestr, "_PRT") ) {
        /* Evaluate _PRT method */
        len = clen;
        ret = 0;
        while ( len > 0 ) {
            ret = _dsdt(parser, d, len);
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
        kprintf("*** %.2x %.2x %.2x %.2x\r\n",
                *d, *(d+1), *(d+2), *(d+3));
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
