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

/* 14.4MiB */
#define FLOPPY_SIZE 1474560
#define MBR_SIZE 512
#define LOADER_SIZE 4096
#define BUFFER_SIZE 512

void
usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s output-image-file mbr loader kernel\r\n"
            "\tAlign and merge `mbr', `loader', and kernel to"
            " `output-image-file'.\r\n"
            "\tThe size of `mbr' must be less than or equal to 510 bytes.\r\n"
            "\tThe size of `loader' must be less than or equal to 4096"
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
    const char *mbrfname;
    const char *loaderfname;
    const char *kernelfname;
    FILE *imgfp;
    FILE *mbrfp;
    FILE *loaderfp;
    FILE *kernelfp;
    off_t cur;
    off_t mbrsize;
    off_t loadersize;
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
    mbrfname = argv[2];
    loaderfname = argv[3];
    kernelfname = argv[4];

    /* Open the output image file */
    imgfp = fopen(imgfname, "wb");
    if ( NULL == imgfp ) {
        fprintf(stderr, "Cannot open %s\n", imgfname);
        return EXIT_FAILURE;
    }

    /* Open the mbr file */
    mbrfp = fopen(mbrfname, "rb");
    if ( NULL == mbrfp ) {
        fprintf(stderr, "Cannot open %s\n", mbrfname);
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

    /* Get the filesize of MBR */
    if ( 0 != fseeko(mbrfp, 0, SEEK_END) ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }
    mbrsize = ftello(mbrfp);
    if ( -1 == mbrsize ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }
    if ( 0 != fseeko(mbrfp, 0, SEEK_SET) ) {
        perror("fseeko");
        return EXIT_FAILURE;
    }

    /* Check the MBR size: 2 bytes for magic */
    if ( mbrsize > MBR_SIZE - 2 ) {
        fprintf(stderr, "Invalid MBR size (MBR must be in 510 bytes)\n");
        return EXIT_FAILURE;
    }

    /* Read MBR */
    done = 0;
    while ( done < mbrsize ) {
        n = fread(buf+done, 1, mbrsize - done, mbrfp);
        done += n;
    }
    /* Write MBR to the image */
    done = 0;
    while ( done < mbrsize ) {
        n = fwrite(buf+done, 1, mbrsize - done, imgfp);
        done += n;
    }

    /* Padding with 0 */
    cur = mbrsize;
    (void)memset(buf, 0, sizeof(buf));
    while ( cur < MBR_SIZE - 2 ) {
        /* Note: Enough buffer size to write */
        nw = fwrite(buf, 1, MBR_SIZE - 2 - cur, imgfp);
        cur += nw;
    }
    /* Write magic (0x55 0xaa) */
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
        fprintf(stderr, "Invalid loader size (loader must be in 4096 bytes)\n");
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
    while ( cur < MBR_SIZE + LOADER_SIZE ) {
        /* Compute the size */
        if ( MBR_SIZE + LOADER_SIZE - cur < sizeof(buf)  ) {
            n = MBR_SIZE + LOADER_SIZE - cur;
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
    (void)fclose(mbrfp);
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
