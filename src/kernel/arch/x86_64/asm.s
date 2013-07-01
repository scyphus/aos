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
 *   Default parameters
 *     %cs = 0x8 (64bit code selector)
 *     %ip = 0x10000
 *  %rax = pointer to the configuration
 */
_kmain:

/* Enable PAE */
	movq	%cr0,%rax
	orl	$0x20,%eax	/* cr4[bit 5] == PAE */
	movq	%rax,%cr0

/* Create 64bit page table */
pg_setup:
	//movl	


	sti

	//movq	%rax
	nop
	jmp	idle



/* Idle process */
idle:
	sti
	hlt
	cli
	jmp	idle

