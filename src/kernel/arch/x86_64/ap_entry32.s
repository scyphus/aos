/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.include	"asmconst.h"
	.file		"ap_entry32.s"

/* Text section */
	.text

	.globl	ap_entry32

	.align	16
	.code32

ap_entry32:
	cli

	/* %cs is automatically set after the long jump operation */
	/* Setup other segment registers */
	movl	$AP_GDT_DATA_SEL,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs

	/* Obtain APIC ID */
	movl	$0xfee00000,%edx
	movl	0x20(%edx),%eax
	shrl	$24,%eax

	/* Setup stack with 16 byte guard */
	addl	$1,%eax
	movl	$P_DATA_SIZE,%ebx
	mull	%ebx			/* [%edx|%eax] = %eax * P_DATA_SIZE */
	addl	$P_DATA_BASE,%eax
	subl	$P_STACK_GUARD,%eax
	movl	%eax,%esp

	/* Enable PAE */
	movl	$0x20,%eax		/* CR4[bit 5] = PAE */
	movl	%eax,%cr4

/* Setup page table register */
	movl	$KERNEL_PGT,%ebx
	movl	%ebx,%cr3

/* Enable long mode */
	movl	$0xc0000080,%ecx	/* EFER MSR number */
	rdmsr				/* Read from 64bit-specific register */
	btsl	$8,%eax			/* LME bit = 1 */
	wrmsr

/* Activate page translation and long mode */
	movl	$0x80000001,%eax
	movl	%eax,%cr0

/* Load code64 descriptor */
	pushl	$AP_GDT_CODE64_SEL
	pushl	$ap_entry64
	lret


