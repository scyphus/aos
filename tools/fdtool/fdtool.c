/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* 14.4MiB */
#define FLOPPY_SIZE 1474560
#define MBR_SIZE 512
#define IPL_SIZE 442
#define LOADER_SIZE 4096 * 8
#define BUFFER_SIZE 512

void
usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s output-image-file ipl loader kernel\r\n"
            "\tAlign and merge `ipl', `loader', and kernel to"
            " `output-image-file'.\r\n"
            "\tThe size of `ipl' must be less than or equal to 442 bytes.\r\n"
            "\tThe size of `loader' must be less than or equal to 32KiB"
            " bytes.\r\n"
            "\tThe overall size must be less than or equal to 14.4MiB.\r\n",
            prog);
    exit(EXIT_FAILURE);
}

int
main(int argc, const char *const argv[])
{
    const char *prog;
    const char *imgfname;
    const char *iplfname;
    const char *loaderfname;
    const char *kernelfname;
    FILE *imgfp;
    FILE *iplfp;
    FILE *loaderfp;
    FILE *kernelfp;
    off_t cur;
    off_t iplsize;
    off_t loadersize;
    off_t kernelsize;
    size_t n;
    size_t nw;
    off_t done;
    unsigned char buf[BUFFER_SIZE];     /* Buffer must be >= 512 bytes */

    /* Get arguments */
    prog = argv[0];
    if ( 5 != argc ) {
        usage(prog);
    }
    imgfname = argv[1];
    iplfname = argv[2];
    loaderfname = argv[3];
    kernelfname = argv[4];

    /* Open the output image file */
    imgfp = fopen(imgfname, "wb");
    if ( NULL == imgfp ) {
        fprintf(stderr, "Cannot open %s\n", imgfname);
        return EXIT_FAILURE;
    }

    /* Open the ipl file */
    iplfp = fopen(iplfname, "rb");
    if ( NULL == iplfp ) {
        fprintf(stderr, "Cannot open %s\n", iplfname);
        return EXIT_FAILURE;
    }

    /* Open the loader file */
    loaderfp = fopen(loaderfname, "rb");
    if ( NULL == loaderfp ) {
        fprintf(stderr, "Cannot open %s\n", loaderfname);
        return EXIT_FAILURE;
    }

    /* Open the kernel file */
    kernelfp = fopen(kernelfname, "rb");
    if ( NULL == loaderfp ) {
        fprintf(stderr, "Cannot open %s\n", kernelfname);
        return EXIT_FAILURE;
    }

    /* Get the filesize of IPL */
    if ( 0 != fseeko(iplfp, 0, SEEK_END) ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }
    iplsize = ftello(iplfp);
    if ( -1 == iplsize ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }
    if ( 0 != fseeko(iplfp, 0, SEEK_SET) ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }

    /* Check the IPL size: 2 bytes for boot signature */
    if ( iplsize > IPL_SIZE ) {
        fprintf(stderr, "Invalid IPL size (IPL must be in 442 bytes)\n");
        return EXIT_FAILURE;
    }

    /* Read IPL */
    done = 0;
    while ( done < iplsize ) {
        n = fread(buf+done, 1, iplsize - done, iplfp);
        done += n;
    }
    /* Write IPL to the image */
    done = 0;
    while ( done < iplsize ) {
        n = fwrite(buf+done, 1, iplsize - done, imgfp);
        done += n;
    }

    cur = iplsize;

    /* Padding with 0 */
    (void)memset(buf, 0, sizeof(buf));
    while ( cur < IPL_SIZE ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf, 1, IPL_SIZE - cur, imgfp);
        cur += nw;
    }

    /* Stage 2 information */
    buf[0] = 1;
    buf[1] = 0;
    buf[2] = 0x40;
    buf[3] = 0;
    done = 0;
    while ( done < 4 ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf+done, 1, 4 - done, imgfp);
        done += nw;
    }
    cur += 4;

    /* Partition entry #1 */
    /*(1023, 255, 63) for GPT*/
    buf[0] = 0x80;
    /* 3 bytes; H[7:0] C[9:8]S[5:0] C[7:0] */
    /* CHS to LBA: LBA = (C * #H + H) * #S + (S - 1) */
    /* #H = 255, #S = 63 */
    buf[1] = 0x02;
    buf[2] = 0x03;
    buf[3] = 0x00;
    /* Partition type (FAT) */
    buf[4] = 0x0b;
    /* Last */
    buf[5] = 0x2d;
    buf[6] = 0x2d;
    buf[7] = 0x00;
    /* LBA of the first sector (little endian) */
    buf[8] = 0x80;
    buf[9] = 0x00;
    buf[10] = 0x00;
    buf[11] = 0x00;
    /* # of sectors */
    buf[12] = 0xc0;
    buf[13] = 0x0a;
    buf[14] = 0x00;
    buf[15] = 0x00;
    done = 0;
    while ( done < 16 ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf + done, 1, 16 - done, imgfp);
        done += nw;
    }
    cur += done;
    /* Partition entry #2 */
    (void)memset(buf, 0, sizeof(buf));
    while ( cur < 478 ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf, 1, 478 - cur, imgfp);
        cur += nw;
    }
    /* Partition entry #3 */
    while ( cur < 494 ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf, 1, 494 - cur, imgfp);
        cur += nw;
    }
    /* Partition entry #4 */
    while ( cur < 510 ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf, 1, 510 - cur, imgfp);
        cur += nw;
    }

    /* Write magic (signature: 0x55 0xaa) */
    if ( EOF == fputc(0x55, imgfp) ) {
        perror("fputc");
        return EXIT_FAILURE;
    }
    cur++;
    if ( EOF == fputc(0xaa, imgfp) ) {
        perror("fputc");
        return EXIT_FAILURE;
    }
    cur++;

    /* Get the filesize of loader */
    if ( 0 != fseeko(loaderfp, 0, SEEK_END) ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }
    loadersize = ftello(loaderfp);
    if ( -1 == loadersize ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }
    if ( 0 != fseeko(loaderfp, 0, SEEK_SET) ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }

    /* Check the filesize of loader */
    if ( loadersize > LOADER_SIZE ) {
        fprintf(stderr, "Invalid loader size (loader must be in 32KiB)\n");
        return EXIT_FAILURE;
    }

    /* Copy loader program */
    while ( !feof(loaderfp) ) {
        n = fread(buf, 1, sizeof(buf), loaderfp);
        /* Write */
        done = 0;
        while ( done < n ) {
            nw = fwrite(buf+done, 1, n - done, imgfp);
            done += nw;
        }
        cur += done;
    }

    /* Padding with 0 */
    (void)memset(buf, 0, sizeof(buf));
    while ( cur < 0x10000 ) {
        /* Compute the size */
        if ( 0x10000 - cur < sizeof(buf)  ) {
            n = 0x10000 - cur;
        } else {
            n = sizeof(buf);
        }
        /* Write */
        done = 0;
        while ( done < n ) {
            nw = fwrite(buf, 1, n - done, imgfp);
            done += nw;
        }
        cur += done;
    }

    /* Create a FAT12 volume */
    /* BS_jmpBoot */
    buf[0] = 0xeb;
    buf[1] = 62;
    buf[2] = 0x90;              /* nop */
    /* BS_OEMName */
    (void)memcpy(buf+3, "AOS  1.0", 8);
    /* BPB_BytsPerSec */
    buf[11] = 0x00;
    buf[12] = 0x02;
    /* BPB_SecPerClus */
    buf[13] = 0x08;
    /* BPB_RsvdSecCnt */
    buf[14] = 0x01;
    buf[15] = 0x00;
    /* BPB_NumFATs */
    buf[16] = 0x02;
    /* BPB_RootEntCnt */
    buf[17] = 0x00;
    buf[18] = 0x02;
    /* BPB_TotSec16 */
    buf[19] = 0xc0;
    buf[20] = 0x0a;
    /* BPB_Media */
    buf[21] = 0xf8;
    /* BPB_FATSz16 */
    buf[22] = 0x08;
    buf[23] = 0x00;
    /* BPB_SecPerTrk */
    buf[24] = 0x20;
    buf[25] = 0x00;
    /* BPB_NumHeads */
    buf[26] = 0x20;
    buf[27] = 0x00;
    /* BPB_HiddSec */
    buf[28] = 0x00;
    buf[29] = 0x08;
    buf[30] = 0x00;
    buf[31] = 0x00;
    /* BPB_TotSec32 */
    buf[32] = 0x00;
    buf[33] = 0x00;
    buf[34] = 0x00;
    buf[35] = 0x00;
    /* BS_DrvNum */
    buf[36] = 0x80;
    /* BS_Reserved1 */
    buf[37] = 0x00;
    /* BS_BootSig */
    buf[38] = 0x29;
    /* BS_VolID */
    time_t timer;
    struct tm *tm;
    time(&timer);
    tm = localtime(&timer);
    buf[39] = (tm->tm_sec/2) | ((tm->tm_min & 0x7) << 5);
    buf[40] = (tm->tm_min >> 3) | (tm->tm_hour << 3);
    buf[41] = tm->tm_mday | ((tm->tm_mon & 0x7) << 5);
    buf[42] = (tm->tm_mon >> 3) | (tm->tm_year - 80);
    /* BS_VolLab */
    memcpy(buf+43, "NO NAME    ", 11);
    /* BS_FilSysType */
    memcpy(buf+54, "FAT12   ", 8);

    /* Write */
    done = 0;
    while ( done < 62 ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf + done, 1, 62 - done, imgfp);
        done += nw;
    }
    (void)memset(buf, 0, sizeof(buf));
    while ( done < 510 ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf, 1, 510 - done, imgfp);
        done += nw;
    }
    cur += done;
    /* Write magic (signature: 0x55 0xaa) */
    if ( EOF == fputc(0x55, imgfp) ) {
        perror("fputc");
        return EXIT_FAILURE;
    }
    cur++;
    if ( EOF == fputc(0xaa, imgfp) ) {
        perror("fputc");
        return EXIT_FAILURE;
    }
    cur++;


    /* Get the filesize of loader */
    if ( 0 != fseeko(kernelfp, 0, SEEK_END) ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }
    kernelsize = ftello(kernelfp);
    if ( -1 == kernelsize ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }
    if ( 0 != fseeko(kernelfp, 0, SEEK_SET) ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }

    /* Cluster size = 8 * 512 = 4096 (4KiB) */
    int clst;
    int next;
    unsigned char cbuf[512 * 32];
    int i;
    clst = (kernelsize - 1) / 4096 + 1;

    for ( i = 0; i < 512 * 8; i++ ) {
        cbuf[i] = 0;
    }

    /* FAT[0], FAT[1] */
    cbuf[0] = 0xf8;
    cbuf[1] = 0xff;
    cbuf[2] = 0xff;

    /* FAT[2]-FAT[clst+1] */
    if ( (512 * 8) * 8 / 12 <= clst + 2 ) {
        /* FAT sectors cannot be in 8 sectors. */
        fprintf(stderr, "FAT size error %d\n", clst);
        return EXIT_FAILURE;
    }

    /* FAT sector */
    for ( i = 2; i < clst + 2; i++ ) {
        if ( i == clst + 1 ) {
            next = 0xfff;
        } else {
            next = i + 1;
        }
        int x = i * 3 / 2;
        if ( i % 2 ) {
            /* Odd */
            cbuf[x] |= (next << 4) & 0xf0;
            cbuf[x+1] = next >> 4;
        } else {
            /* Even */
            cbuf[x] = next & 0xff;
            cbuf[x+1] |= (next >> 8) & 0xf;
        }
    }
    done = 0;
    while ( done < 512 * 8 ) {
        nw = fwrite(cbuf+done, 1, 512 * 8 - done, imgfp);
        done += nw;
    }
    cur += done;

    /* One more */
    done = 0;
    while ( done < 512 * 8 ) {
        nw = fwrite(cbuf+done, 1, 512 * 8 - done, imgfp);
        done += nw;
    }
    cur += done;

    /* Root */
    (void)memset(cbuf, 0, 512 * 32);
    memcpy(cbuf, "NO NAME    ", 11);
    cbuf[11] = 0x08;
    cbuf[12] = 0x00;
    cbuf[13] = 0x00;
    cbuf[22] = (tm->tm_sec/2) | ((tm->tm_min & 0x7) << 5);
    cbuf[23] = (tm->tm_min >> 3) | (tm->tm_hour << 3);
    cbuf[24] = tm->tm_mday | ((tm->tm_mon & 0x7) << 5);
    cbuf[25] = (tm->tm_mon >> 3) | (tm->tm_year - 80);
    memcpy(cbuf+32, "KERNEL     ", 11);
    cbuf[43] = 0x01/* | 0x04*/;
    cbuf[44] = 0x00;
    cbuf[45] = 0x00;
    cbuf[54] = (tm->tm_sec/2) | ((tm->tm_min & 0x7) << 5);
    cbuf[55] = (tm->tm_min >> 3) | (tm->tm_hour << 3);
    cbuf[56] = tm->tm_mday | ((tm->tm_mon & 0x7) << 5);
    cbuf[57] = (tm->tm_mon >> 3) | (tm->tm_year - 80);
    cbuf[58] = 0x02;
    cbuf[59] = 0x00;
    cbuf[60] = kernelsize & 0xff;
    cbuf[61] = (kernelsize >> 8) & 0xff;
    cbuf[62] = (kernelsize >> 16) & 0xff;
    cbuf[63] = (kernelsize >> 24) & 0xff;
    done = 0;
    while ( done < 512 * 32 ) {
        nw = fwrite(cbuf+done, 1, 512 * 32 - done, imgfp);
        done += nw;
    }
    cur += done;

    /* Copy kernel program */
    while ( !feof(kernelfp) ) {
        n = fread(buf, 1, sizeof(buf), kernelfp);
        /* Write */
        done = 0;
        while ( done < n ) {
            nw = fwrite(buf+done, 1, n - done, imgfp);
            done += nw;
        }
        cur += done;
        if ( cur > FLOPPY_SIZE ) {
            fprintf(stderr, "Exceeds floppy size\n");
            return EXIT_FAILURE;
        }
    }


    /* Padding with zero */
    (void)memset(buf, 0, sizeof(buf));
    while ( cur < FLOPPY_SIZE ) {
        if ( FLOPPY_SIZE - cur < sizeof(buf) ) {
            nw = fwrite(buf, 1, FLOPPY_SIZE - cur, imgfp);
        } else {
            nw = fwrite(buf, 1, sizeof(buf), imgfp);
        }
        cur += nw;
    }


    /* Close the file */
    (void)fclose(imgfp);
    (void)fclose(iplfp);
    (void)fclose(loaderfp);
    (void)fclose(kernelfp);

    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
