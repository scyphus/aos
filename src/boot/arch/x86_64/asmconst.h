/* -*- Mode: asm -*- */
/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */
	.set	BOOTINFO_BASE,0x8000	/* Boot information base address */
	.set	BOOTINFO_SIZE,0x100	/* Size of boot info structure */
	.set	MME_SIZE,24		/* Memory map entry size */
	.set	MME_SIGN,0x534d4150	/* MME signature (ascii "SMAP")  */

	.set	VGA_TEXT_COLOR_80x25,0x03

	.set	IVT_IRQ0,0x08		/* IRQ0=0x08 (BIOS default) */
	.set	IVT_IRQ8,0x70		/* IRQ8=0x70 (BIOS default) */

	.set	KERNEL_SEG,0x1000	/* Memory where to load kernel */


