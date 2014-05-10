/*_
 * Copyright 2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

#include "boot.h"

#define FAT12 12
#define FAT16 16
#define FAT32 32


struct fat_bpb_common {
    u8 bs_jmp_boot[3];
    u8 bs_oem_name[8];
    u16 bpb_bytes_per_sec;
    u8 bpb_sec_per_clus;
    u16 bpb_rsvd_sec_cnt;
    u8 bpb_num_fats;
    u16 bpb_root_ent_cnt;
    u16 bpb_total_sec16;
    u8 bpb_media;
    u16 bpb_fat_sz16;
    u16 bpb_sec_per_trk;
    u16 bpb_num_heads;
    u32 bpb_hidden_sec;
    u32 bpb_total_sec32;
} __attribute__ ((packed));

struct fat_bpb_fat12_16 {
    struct fat_bpb_common common;
    u8 bs_drv_num;
    u8 bs_reserved1;
    u8 bs_boot_sig;
    u32 bs_vol_id;
    u8 bs_vol_lab[11];
    u8 bs_file_sys_type[8];
} __attribute__ ((packed));
struct fat_bpb_fat32 {
    struct fat_bpb_common common;
    u32 bpb_fat_sz32;
    u16 bpb_ext_flags;
    u16 bpb_fs_ver;
    u32 bpb_root_clus;
    u16 bpb_fs_info;
    u16 bpb_bk_boot_sec;
    u8 bpb_reserved[12];
    u8 bs_drv_num;
    u8 bs_reserved1;
    u8 bs_boot_sig;
    u32 bs_vol_id;
    u8 bs_vol_lab[11];
    u8 bs_file_sys_type[8];
} __attribute__ ((packed));


/* Detect the type of FAT */
int
fat_detect_type(const u8 *buf)
{
    struct fat_bpb_common *bpb;
    struct fat_bpb_fat32 *bpb32;
    int start_sec;
    int sectors;
    int root_dir_start_sec;
    int root_dir_sectors;
    int data_start_sec;
    int data_sectors;
    int num_clus;

    bpb = (struct fat_bpb_common *)buf;

    if ( 0 == bpb->bpb_fat_sz16 ) {
        /* Expect FAT32 */
        bpb32 = (struct fat_bpb_fat32 *)bpb;
        start_sec = bpb32->common.bpb_rsvd_sec_cnt;
        if ( bpb32->common.bpb_fat_sz16 != 0 ) {
            /* Invalid format */
            return -1;
        }
        sectors = bpb32->bpb_fat_sz32 * bpb32->common.bpb_num_fats;
        root_dir_start_sec = start_sec + sectors;
        if ( bpb32->common.bpb_root_ent_cnt != 0 ) {
            /* Invalid format */
            return -1;
        }
        root_dir_sectors = 0;
        data_start_sec = root_dir_start_sec + root_dir_sectors;
        if ( bpb32->common.bpb_total_sec16 != 0 ) {
            /* Invalid format */
            return -1;
        }
        data_sectors = bpb32->common.bpb_total_sec32 - data_start_sec;

        /* Calculate the number of clusters */
        num_clus = data_sectors / bpb->bpb_sec_per_clus;
        if ( num_clus < 65526 ) {
            return -1;
        }
        return FAT32;
    } else {
        /* Expect FAT12/16 */
        start_sec = bpb->bpb_rsvd_sec_cnt;
        sectors = bpb->bpb_fat_sz16 * bpb->bpb_num_fats;
        root_dir_start_sec = start_sec + sectors;
        root_dir_sectors = (32 * bpb->bpb_root_ent_cnt + bpb->bpb_bytes_per_sec
                            - 1) / bpb->bpb_bytes_per_sec;
        data_start_sec = root_dir_start_sec + root_dir_sectors;
        if ( bpb->bpb_total_sec16 != 0 ) {
            /* The number of sectors is less than 0x10000. */
            data_sectors = bpb->bpb_total_sec16 - data_start_sec;
        } else {
            /* The number of sectors is no less than 0x10000. */
            data_sectors = bpb->bpb_total_sec32 - data_start_sec;
        }

        /* Calculate the number of clusters */
        num_clus = data_sectors / bpb->bpb_sec_per_clus;
        if ( num_clus <= 4085 ) {
            return FAT12;
        } else if ( num_clus <= 65525 ) {
            return FAT16;
        } else {
            return -1;
        }
    }

    return -1;
}

int
bmemcmp(const u8 *a, const u8 *b, int n)
{
    while ( n > 0 ) {
        if ( *a > *b ) {
            return 1;
        } else if ( *a < *b ) {
            return -1;
        }
        a++;
        b++;
        n--;
    }

    return 0;
}

int
fat_load_kernel(struct blkdev *blkdev, u64 addr)
{
    struct fat_bpb_fat12_16 *bpb;
    struct fat_bpb_fat32 *bpb32;
    int start_sec;
    int sectors;
    int root_dir_start_sec;
    int root_dir_start_bytes;
    int root_dir_sectors;
    int root_dir_bytes;
    int data_start_sec;
    int data_sectors;
    int num_clus;
    u8 *buf;
    u8 *cluster;
    int ret;
    int fattype;
    int i;

    /* Load MBR */
    buf = bmalloc(4096);
    ret = blkdev->read(blkdev, 0, 8, buf);
    if ( ret < 0 ) {
        bfree(buf);
        return -1;
    }
    bfree(buf);

    u32 lba = *(u32 *)(buf + 0x1be + 0x8);
    u32 nsec = *(u32 *)(buf + 0x1be + 0xc);

    bpb = (struct fat_bpb_common *)bmalloc(4096);
    ret = blkdev->read(blkdev, lba, 8, bpb);
    if ( ret < 0 ) {
        bfree(bpb);
        return -1;
    }

    if ( 0 == bpb->common.bpb_fat_sz16 ) {
        /* Expect FAT32 */
        bpb32 = (struct fat_bpb_fat32 *)bpb;
        if ( bpb32->common.bpb_fat_sz16 != 0 ) {
            /* Invalid format */
            bfree(bpb);
            return -1;
        }
        sectors = bpb32->bpb_fat_sz32 * bpb32->common.bpb_num_fats;
        root_dir_start_sec = start_sec + sectors;
        if ( bpb32->common.bpb_root_ent_cnt != 0 ) {
            /* Invalid format */
            bfree(bpb);
            return -1;
        }
        root_dir_sectors = 0;
        data_start_sec = root_dir_start_sec + root_dir_sectors;
        if ( bpb32->common.bpb_total_sec16 != 0 ) {
            /* Invalid format */
            bfree(bpb);
            return -1;
        }
        data_sectors = bpb32->common.bpb_total_sec32 - data_start_sec;

        /* Calculate the number of clusters */
        num_clus = data_sectors / bpb->common.bpb_sec_per_clus;
        if ( num_clus < 65526 ) {
            bfree(bpb);
            return -1;
        }
        fattype = FAT32;
    } else {
        /* Expect FAT12/16 */
        start_sec = bpb->common.bpb_rsvd_sec_cnt;
        sectors = bpb->common.bpb_fat_sz16 * bpb->common.bpb_num_fats;
        root_dir_start_sec = start_sec + sectors;
        root_dir_sectors = (32 * bpb->common.bpb_root_ent_cnt
                            + bpb->common.bpb_bytes_per_sec
                            - 1) / bpb->common.bpb_bytes_per_sec;
        data_start_sec = root_dir_start_sec + root_dir_sectors;
        if ( bpb->common.bpb_total_sec16 != 0 ) {
            /* The number of sectors is less than 0x10000. */
            data_sectors = bpb->common.bpb_total_sec16 - data_start_sec;
        } else {
            /* The number of sectors is no less than 0x10000. */
            data_sectors = bpb->common.bpb_total_sec32 - data_start_sec;
        }

        /* Calculate the number of clusters */
        num_clus = data_sectors / bpb->common.bpb_sec_per_clus;
        if ( num_clus <= 4085 ) {
            fattype = FAT12;
        } else if ( num_clus <= 65525 ) {
            fattype = FAT16;
        } else {
            bfree(bpb);
            return -1;
        }
    }

    /* For cluster */
    cluster = bmalloc(sectors * bpb->common.bpb_bytes_per_sec);
    ret = blkdev->read(blkdev, lba +
                       (start_sec * bpb->common.bpb_bytes_per_sec) / 512,
                       (sectors * bpb->common.bpb_bytes_per_sec) / 512,
                       cluster);
    if ( ret < 0 ) {
        bfree(cluster);
        bfree(bpb);
        return -1;
    }

    switch ( fattype ) {
    case FAT12:
        /* Allocate memory */
        root_dir_bytes = root_dir_sectors * bpb->common.bpb_bytes_per_sec;
        root_dir_start_bytes = root_dir_start_sec * bpb->common.bpb_bytes_per_sec;
        buf = bmalloc(root_dir_bytes);
        /* Root directory */
        ret = blkdev->read(blkdev, lba + root_dir_start_bytes / 512,
                           root_dir_bytes / 512, buf);
        if ( ret < 0 ) {
            bfree(buf);
            bfree(bpb);
            bfree(cluster);
            return -1;
        }

        for ( i = 0; i < root_dir_bytes / 32; i++ ) {
            /* Find out the file named KERNEL from the root directory */
            if ( 0 == bmemcmp(buf + i * 32, "KERNEL     ", 11) ) {
                /* Found */
                u16 cl = *(u16 *)(buf + i * 32 + 26);
                u32 sz = *(u32 *)(buf + i * 32 + 28);
                u16 next;
                int start = 0;

                next = cl;
                for ( ;; ) {
                    cl = next;
                    if ( cl % 2 ) {
                        /* Odd */
                        next = (((u16)cluster[cl * 3 / 2] & 0xf0) >> 4)
                            | ((u16)cluster[cl * 3 / 2 + 1] << 4);
                    } else {
                        /* Even */
                        next = (u16)cluster[cl * 3 / 2]
                            | (((u16)cluster[cl * 3 / 2 + 1] & 0xf) << 8);
                    }
                    blkdev->read(blkdev, lba
                                 + (bpb->common.bpb_bytes_per_sec * data_start_sec)
                                 / 512 + (bpb->common.bpb_sec_per_clus * (cl - 2)
                                          * bpb->common.bpb_bytes_per_sec) / 512,
                                 (bpb->common.bpb_sec_per_clus
                                  * bpb->common.bpb_bytes_per_sec) / 512,
                                 (u8 *)(addr + start
                                        * bpb->common.bpb_sec_per_clus
                                        * bpb->common.bpb_bytes_per_sec));
                    start++;
                    if ( next >= 0xff8 ) {
                        break;
                    }
                }
                bfree(buf);
                bfree(bpb);
                bfree(cluster);

                return 0;
            }
        }
        break;
    case FAT16:
        /* Allocate memory */
        root_dir_bytes = root_dir_sectors * bpb->common.bpb_bytes_per_sec;
        root_dir_start_bytes = root_dir_start_sec * bpb->common.bpb_bytes_per_sec;
        buf = bmalloc(root_dir_bytes);
        /* Root directory */
        ret = blkdev->read(blkdev, lba + root_dir_start_bytes / 512,
                           root_dir_bytes / 512, buf);
        if ( ret < 0 ) {
            bfree(buf);
            bfree(bpb);
            bfree(cluster);
            return -1;
        }

        for ( i = 0; i < root_dir_bytes / 32; i++ ) {
            /* Find out the file named KERNEL from the root directory */
            if ( 0 == bmemcmp(buf + i * 32, "KERNEL     ", 11) ) {
                /* Found */
                u16 cl = *(u16 *)(buf + i * 32 + 26);
                u32 sz = *(u32 *)(buf + i * 32 + 28);
                u16 next;
                int start = 0;

                next = cl;
                for ( ;; ) {
                    cl = next;
                    next = (u16)cluster[cl * 2] | (u16)cluster[cl * 2 + 1];
                    blkdev->read(blkdev, lba
                                 + (bpb->common.bpb_bytes_per_sec * data_start_sec)
                                 / 512 + (bpb->common.bpb_sec_per_clus * (cl - 2)
                                          * bpb->common.bpb_bytes_per_sec) / 512,
                                 (bpb->common.bpb_sec_per_clus
                                  * bpb->common.bpb_bytes_per_sec) / 512,
                                 (u8 *)(addr + start
                                        * bpb->common.bpb_sec_per_clus
                                        * bpb->common.bpb_bytes_per_sec));
                    start++;
                    if ( next >= 0xfff8 ) {
                        break;
                    }
                }
                bfree(buf);
                bfree(bpb);
                bfree(cluster);

                return 0;
            }
        }
        break;
    default:
        /* Must support FAT32 */
        ;
    }


    bfree(buf);
    bfree(bpb);
    bfree(cluster);

    return -1;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
