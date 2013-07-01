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

	.set	IVT_IRQ0,0x20		/* IRQ0=0x20 */
	.set	IVT_IRQ8,0x28		/* IRQ8=0x28 */


	.set	GDT_CODE64_SEL,0x08	/* Code64 selector */
	.set	GDT_CODE32_SEL,0x10	/* Code32 selector */
	.set	GDT_CODE16_SEL,0x18	/* Code16 selector */
	.set	GDT_DATA_SEL,0x20	/* Data selector */

	.set	KERNEL_STACK,0x7c00	/* Kernel stack */

	.set	KERNEL_SEG,0x1000	/* Memory where to load kernel */
	.set	KERNEL_OFF,0x0000	/*  segment and offset [1000:0000] */

	.set	KERNEL_STACK,0x00078ff0	/* Kernel stack */
	.set	KERNEL_PGT,0x00079000	/* Page table */
