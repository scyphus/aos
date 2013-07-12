/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.include	"asmconst.h"
	.file		"ap_entry64.s"

/* Text section */
	.text

	.globl	ap_entry64

	.align	16
	.code64

ap_entry64:
	cli
	xorl	%eax,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	movl	%eax,%fs
	movl	%eax,%gs

	jmp	apstart64




