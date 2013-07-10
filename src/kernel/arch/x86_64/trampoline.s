/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.file	"tranmpoline.s"

/* Text section */
	.text

	.globl	_trampoline
	.globl	_trampoline_end

	.code16

_trampoline:
	cli

	movw	%cs,%ax
	movw	%ax,%ds

	/* Setup GDT and IDT */
	//lgdt
	//lidt

	movl	%cr0,%eax
	orb	$0x1,%al	/* Enable protected mode */
	movl	%eax,%cr0
	//ljmp	$GDT_CODE32_SEL,$entry32	/* Go into protected mode */
	movl	%eax,%dr0
1:	hlt
	jmp	1b

_trampoline_end:

