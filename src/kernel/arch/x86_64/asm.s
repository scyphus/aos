/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.file	"asm.s"

	.text
	.globl	_kmain

	.code64

/*
 * Kernel main function
 */
_kmain:
	call	_cstart
	jmp	idle


/* Idle process */
idle:
	sti
	hlt
	cli
	jmp	idle

