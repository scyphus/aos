/*_
 * Copyright 2013-2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.include	"asmconst.h"
	.file		"entry16.s"

	.globl	entry16

	.text
	.code16

/* Entry point */
entry16:
	/* Copy %cs to %ds */
	xorl	%eax,%eax

	/* Setup temporary IDT and GDT */
	lidt	(idtr)
	lgdt	(gdtr)

	movl	%cr0,%eax
	orb	$0x1,%al	/* Enable protected mode */
	movl	%eax,%cr0

	/* Go into protected mode and flush the pipeline */
	ljmpl	$GDT_CODE32_SEL,$entry32

halt16:
	hlt
	jmp	halt16



/* Data section */
	.align	16
	//.data

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


