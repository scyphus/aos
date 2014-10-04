/*_
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

	.include	"asmconst.h"
	.file		"pxeboot.s"

	.set	PXE_SIGNATURE,0x45585021
	.set	KERNEL_ADDR,0x10000

	.text
	.code16
	.globl	pxeboot

/* Entry point */
pxeboot:
	cld

	cli
	xorw	%ax,%ax
/* Reset %ds */
	movw	%ax,%ds
/* Store PXENV+ segment:offset */
	movw	%es,pxenv_seg
	movw	%bx,pxenv_off
	//movw	%ss,pxeopt_seg
	//movw	%sp,pxeopt_off
/* Obtain !PXE segment:offset */
	movw	%ss,%ax
	movw	%ax,%es
	movw	%sp,%bx
	addw	$4,%bx
	movl	%es:(%bx),%eax
	movw	%ax,pxe_off
	shrl	$16,%eax
	movw	%ax,pxe_seg

	sti

/* Parse !PXE structure */
	movw	pxe_seg,%ax
	movw	%ax,%es
	movw	pxe_off,%bx
	movl	%es:(%bx),%eax

	/* !PXE */
	cmpl	$PXE_SIGNATURE,%eax
	jne	error	/* Invalid signature */

	/* Obtain RMEntry */
	movl	%es:16(%bx),%eax
	movl	%eax,pm_entry
	movw	%ax,pm_entry_off
	shrl	$16,%eax
	movw	%ax,pm_entry_seg

/* Get cache information */
	movw	$t_PXENV_GET_CACHED,%di
	movw	$0x0071,%bx
	call	pxeapi
	movw	t_PXENV_GET_CACHED.Status,%ax
	testw	%ax,%ax
	jnz	error
	/* bootph */
	movw	t_PXENV_GET_CACHED.BufferSize,%cx
	movw	t_PXENV_GET_CACHED.BufferOff,%bx
	movw	t_PXENV_GET_CACHED.BufferSeg,%dx

/* Open a file */
	movw	%dx,%es
	movl	%es:20(%bx),%eax
	movl	%eax,t_PXENV_TFTP_OPEN.SIP
	movl	%es:24(%bx),%eax
	movl	%eax,t_PXENV_TFTP_OPEN.GIP
	movw	$69,%ax
	xchgb	%al,%ah
	movw	%ax,t_PXENV_TFTP_OPEN.Port
	movw	$512,%ax
	movw	%ax,t_PXENV_TFTP_OPEN.PacketSize

	movw	$ssfile,%si
	movw	$t_PXENV_TFTP_OPEN.Filename,%di
1:
	movb	(%si),%al
	testb	%al,%al
	jz	2f
	movb	%al,(%di)
	incw	%si
	incw	%di
	jmp	1b
2:
	movw	$t_PXENV_TFTP_OPEN,%di
	movw	$0x0020,%bx
	call	pxeapi
	movw	t_PXENV_TFTP_OPEN.Status,%ax
	testw	%ax,%ax
	jnz	error


/* Read the file */
read:
	movw	$t_PXENV_TFTP_READ,%di
	movw	$0x0022,%bx
	movl	$KERNEL_ADDR,%edx
	xorw	%cx,%cx
read.loop:
	movw	%dx,%ax
	andw	$0xf,%ax
	movw	%ax,t_PXENV_TFTP_READ.BufferOff
	movl	%edx,%eax
	shrl	$4,%eax
	movw	%ax,t_PXENV_TFTP_READ.BufferSeg
	call	pxeapi
	movw	t_PXENV_TFTP_READ.Status,%ax
	testw	%ax,%ax
	jnz	error
	/* Check the packet number */
	incw	%cx
	cmpw	t_PXENV_TFTP_READ.PacketNumber,%cx
	jne	error
	/* Check the size */
	xorl	%eax,%eax
	movw	t_PXENV_TFTP_READ.BufferSize,%ax
	addl	%eax,%edx
	cmpl	$0x100000,%edx
	jge	error
	/* Check if it is the last packet */
	cmpw	t_PXENV_TFTP_OPEN.PacketSize,%ax
	jge	read.loop

/* Close */
	movw	$t_PXENV_TFTP_CLOSE,%di
	movw	$0x0021,%bx
	call	pxeapi
	movw	t_PXENV_TFTP_CLOSE.Status,%ax
	testw	%ax,%ax
	jnz	error


bootmon:
	/* Reset the boot information structure */
	xorl	%eax,%eax
	movl	$BOOTINFO_BASE,%ebx
	movl	$BOOTINFO_SIZE,%ecx
	shrl	$2,%ecx
1:	movl	%eax,(%ebx)
	addl	$4,%ebx
	loop	1b

	/* Load memory map entries */
	movw	%ax,%es
	movw	$BOOTINFO_BASE+BOOTINFO_SIZE,%ax
	movw	%ax,(BOOTINFO_BASE+8)
	movw	%ax,%di
	call	load_mm		/* Load system address map to %es:%di */
	movw	%ax,(BOOTINFO_BASE)
	jmp	boot16

/*
 * Load memory map entries from BIOS
 *   Input:
 *     %es:%di: Destination
 *   Return:
 *     %ax: The number of entries
 */
load_mm:
	pushl	%ebx
	pushl	%ecx
	pushw	%di
	pushw	%bp
	xorl	%ebx,%ebx		/* Continuation value for int 0x15 */
	xorw	%bp,%bp			/* Counter */
load_mm.1:
	movl	$0x1,%ecx		/* Write 1 once */
	movl	%ecx,%es:20(%di)	/*  to check support ACPI >=3.x? */
/* Read the system address map */
	movl	$0xe820,%eax
	movl	$MME_SIGN,%edx		/* Set the signature */
	movl	$MME_SIZE,%ecx		/* Set the buffer size */
	int	$0x15			/* Query system address map */
	jc	load_mm.error		/* Error */
	cmpl	$MME_SIGN,%eax		/* Check the signature SMAP */
	jne	load_mm.error

	cmpl	$24,%ecx		/* Check the read buffer size */
	je	load_mm.2		/*  %ecx==24 */
	cmpl	$20,%ecx
	je	load_mm.3		/*  %ecx==20 */
	jmp	load_mm.error		/* Error otherwise */
load_mm.2:
/* 24-byte entry */
	testl	$0x1,%es:20(%di)	/* 1 must be present in the attribute */
	jz	load_mm.error		/*  error if it's overwritten */
load_mm.3:
/* 20-byte entry or 24-byte entry coming from above */
	incw	%bp			/* Increment the number of entries */
	testl	%ebx,%ebx		/* %ebx=0: No remaining info */
	jz	load_mm.done		/* jz/je */
load_mm.4:
	addw	$MME_SIZE,%di		/* Next entry */
	jmp	load_mm.1		/* Load remaining entries */
load_mm.error:
	stc				/* Set CF */
load_mm.done:
	movw	%bp,%ax			/* Return value */
	popw	%bp
	popw	%di
	popl	%ecx
	popl	%ebx
	ret



boot16:
	/* Setup temporary IDT and GDT */
	lidt	(idtr)
	lgdt	(gdtr)

	/* Enable protected mode */
	movl	%cr0,%eax
	orb	$0x1,%al
	movl	%eax,%cr0

	/* Go into protected mode and flush the pipeline */
	ljmpl	$GDT_CODE32_SEL,$boot32


pxeapi:
	push	%ds
	push	%di
	push	%bx
	lcall	*(pm_entry)
	addw	$6,%sp
	ret

error:
	jmp	halt16

halt16:
	hlt
	jmp	halt16



/* Entry point for 32bit protected mode */
	.align	16
	.code32
boot32:
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
	movl	$0x20,%eax		/* CR4[bit 5] = PAE */
	movl	%eax,%cr4

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
	pushl	$boot64
	lret


/* Entry point for 64-bit long mode code */
	.align	16
	.code64
boot64:
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
	movq	$KERNEL_ADDR,%rdi
	call	ljmp64
	jmp	halt64

/* Halt (64bit mode) */
halt64:
	hlt
	jmp	halt64

ljmp64:
	/* Load code64 descriptor */
	pushq	$GDT_CODE64_SEL
	pushq	%rdi
	lretq

/* Data section */
	.align	16
	.data

/* PXE information */
pxenv_seg:
	.word	0x0
pxenv_off:
	.word	0x0
pxe_seg:
	.word	0x0
pxe_off:
	.word	0x0

pm_entry:
	.long	0x0
pm_entry_seg:
	.word	0x0
pm_entry_off:
	.word	0x0

//pxeopt_seg:
//	.word	0x0
//pxeopt_off:
//	.word	0x0


t_PXENV_GET_CACHED:
t_PXENV_GET_CACHED.Status:
	.word	0
t_PXENV_GET_CACHED.PacketType:
	.word	2
t_PXENV_GET_CACHED.BufferSize:
	.word	0
t_PXENV_GET_CACHED.BufferOff:
	.word	0
t_PXENV_GET_CACHED.BufferSeg:
	.word	0
t_PXENV_GET_CACHED.BufferLimit:
	.word	0

t_PXENV_TFTP_OPEN:
t_PXENV_TFTP_OPEN.Status:
	.word	0
t_PXENV_TFTP_OPEN.SIP:
	.long	0
t_PXENV_TFTP_OPEN.GIP:
	.long	0
t_PXENV_TFTP_OPEN.Filename:
	.rept 128
	.byte	0
	.endr
t_PXENV_TFTP_OPEN.Port:
	.word	0
t_PXENV_TFTP_OPEN.PacketSize:
	.word	0

t_PXENV_TFTP_READ:
t_PXENV_TFTP_READ.Status:
	.word	0
t_PXENV_TFTP_READ.PacketNumber:
	.word	0
t_PXENV_TFTP_READ.BufferSize:
	.word	0
t_PXENV_TFTP_READ.BufferOff:
	.word	0
t_PXENV_TFTP_READ.BufferSeg:
	.word	0

t_PXENV_TFTP_CLOSE:
t_PXENV_TFTP_CLOSE.Status:
	.word	0

/* Second stage file name */
ssfile:
	.asciz	"kernel"


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


