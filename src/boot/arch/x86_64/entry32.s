/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.include	"asmconst.h"
	.file		"entry32.s"

/* Text section */
	.text

	.globl	entry32		/* Entry point */


	.align	16
	.code32
/* Entry point for 32bit protected mode */
entry32:
	/* %cs is automatically set after the long jump operation */
	/* Setup stack */
	movl	$GDT_DATA_SEL,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs
	movl	$KERNEL_STACK,%esp

	/* Enable PAE */
	movl	$0x20,%eax		/* CR4[bit 5] = PAE */
	movl	%eax,%cr4

/* Create 64bit page table */
pg_setup:
	movl	$KERNEL_PGT,%ebx	/* Low 12 bit must be zero */
	movl	%ebx,%edi
	xorl	%eax,%eax
	movl	$(512*8*6/4),%ecx
	rep	stosl			/* Initialize %ecx bytes from %edi */
					/*  with %eax */
/* Level 4 page map */
	leal	0x1007(%ebx),%eax
	movl	%eax,(%ebx)
/* Page directory pointers (PDPE) */
	leal	0x1000(%ebx),%edi
	leal	0x2007(%ebx),%eax
	movl	$4,%ecx
pg_setup.1:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x1000,%eax
	loop	pg_setup.1
/* Page directories (PDE) */
	leal	0x2000(%ebx),%edi
	movl	$0x183,%eax
	movl	$(512*4),%ecx
pg_setup.2:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x00200000,%eax
	loop	pg_setup.2

/* Setup page table register */
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
	pushl	$GDT_CODE64_SEL
	pushl	$entry64
	lret



/* Halt (32bit mode) */
halt32:
	hlt
	jmp	halt32

