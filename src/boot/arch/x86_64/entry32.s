/*_
 * Copyright 2013-2014 Scyphus Solutions Co. Ltd.  All rights reserved.
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
	cli

	/* Mask all interrupts (i8259) */
	movb	$0xff,%al
	outb	%al,$0x21
	movb	$0xff,%al
	outb	%al,$0xa1

	/* %cs is automatically set after the long jump operation */
	movl	$GDT_DATA_SEL,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs

	/* Obtain APIC ID (assuming it is 0) */
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
	/* Enable PAE and SSE */
	movl	$0x00000220,%eax        /* CR4[bit 5] = PAE */
	movl	%eax,%cr4               /* CR4[bit 9] = OSFXSR */

/* Create 64bit page table */
pg_setup:
	movl	$KERNEL_PGT,%ebx	/* Low 12 bit must be zero */
	movl	%ebx,%edi
	xorl	%eax,%eax
	movl	$(512*8*6/4),%ecx
	rep	stosl			/* Initialize %ecx*4 bytes from %edi */
					/*  with %eax */
/* Level 4 page map */
	leal	0x1007(%ebx),%eax
	movl	%eax,(%ebx)
/* Page directory pointers (PDPE) */
	leal	0x1000(%ebx),%edi
	leal	0x2007(%ebx),%eax
	movl	$4,%ecx
pg_setup.1a:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x1000,%eax
	loop	pg_setup.1a
	movl	$508,%ecx
	/* PDPE: 4G-512G */
	movl	$0x183,%eax
pg_setup.1b:
	movl	%eax,(%edi)
	addl	$8,%edi
	loop	pg_setup.1b
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

