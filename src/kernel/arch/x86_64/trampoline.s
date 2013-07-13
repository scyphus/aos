/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.include	"asmconst.h"
	.file		"tranmpoline.s"

/* Text section */
	.text

	.globl	_trampoline
	.globl	_trampoline_end

	.code16

_trampoline:
	cli

	/* For debugging */
	movw	$0xb800,%ax
	movw	%ax,%ds
	movw	$0x0778,%ax
	movw	%ax,0xf9e

	movw	%cs,%ax
	movw	%ax,%ds

	/* Setup GDT and IDT */
	lidt	(idtr-_trampoline)
	lgdt	(gdtr-_trampoline)
	/* Turn on protected mode */
	movl	%cr0,%eax
	orb	$0x1,%al	/* Enable protected mode */
	movl	%eax,%cr0
	ljmpl	$AP_GDT_CODE32_SEL,$ap_entry32	/* Go into protected mode */

/* Data section */
	.align	16
	.data

/* Pseudo interrupt descriptor table */
idtr:
	.word	0x0
	.long	0x0

gdt:
	.word	0x0,0x0,0x0,0x0		/* Null entry */
	.word	0xffff,0x0,0x9a00,0xaf	/* Code64 */
	.word	0xffff,0x0,0x9a00,0xcf	/* Code32 */
	.word	0xffff,0x0,0x9a00,0x8f	/* Code16 */
	.word	0xffff,0x0,0x9200,0xcf	/* Data */
gdt.1:
/* Pseudo global descriptor table register */
gdtr:
	.word	gdt.1-gdt-1		/* Limit */
	.long	gdt			/* Address */

_trampoline_end:
