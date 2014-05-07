/*_
 * Copyright 2013-2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.include	"asmconst.h"
	.file		"entry64.s"

/* Text section */
	.text

	.globl	entry64		/* Entry point */
	.globl	_ljmp64


	.align	16
	.code64

/* Entry point for 64-bit long mode code */
entry64:
	cli
	xorl	%eax,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	movl	%eax,%fs
	movl	%eax,%gs

	/* Clear screen */
	movl	$0xb8000,%edi
	movw	$0x0f20,%ax
	movl	$80*25,%ecx
	rep	stosw

	/* Reset cursor */
	movw	$0x000f,%ax	/* %al=0xf: cursor location low, %ah: xx00 */
	movw	$0x3d4,%dx
	outw	%ax,%dx
	movw	$0x000e,%ax	/* %al=0xe: cursor location high, %ah: 00xx */
	movw	$0x3d4,%dx
	outw	%ax,%dx

	/* Jump to the kernel main function */
	call	_cboot
	jmp	halt64

_ljmp64:
	/* Load code64 descriptor */
	pushq	$GDT_CODE64_SEL
	pushq	%rdi
	lretq


/* Halt (64bit mode) */
halt64:
	hlt
	jmp	halt64
