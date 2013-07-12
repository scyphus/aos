/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.file	"ap_asm.s"

/* Text section */
	.text

	.globl	ap_entry32

	.align	16
	.code32

ap_entry32:
	cli

	/* Enable PAE */
	movl	$0x20,%eax		/* CR4[bit 5] = PAE */
	movl	%eax,%cr4

1:	hlt
	jmp	1b
