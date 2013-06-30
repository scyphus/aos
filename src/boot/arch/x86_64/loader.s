/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.set	KERNEL_SEG,0x0900	/* Memory where to load kernel loader */
	.set	KERNEL_OFF,0x0000	/*  segment and offset [0900:0000] */
	.set	KERNEL_SIZE,0x8		/* Size of kernel loader in sectors */
	.set	SECTOR_SIZE,0x200       /* 0x200 for FD, 0x800 for CD */

	.file	"loader.s"

/* Text section */
	.text

	.code16			/* 16bit real mode */
	.globl	loader		/* Entry point */

/* Kernel loader */
loader:
	nop
	jmp	halt

/* Idle process */
idle:
	sti
	hlt
	cli
	jmp	idle

/* Halt */
halt:
	hlt
	jmp	halt
