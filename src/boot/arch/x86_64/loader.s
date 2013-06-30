/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.set	BOOTINFO_BASE,0x8000	/* Boot information base address */
	.set	BOOTINFO_SIZE,0x100	/* Size of boot info structure */
	.set	MME_SIZE,24		/* Memory map entry size */
	.set	MME_SIGN,0x534d4150	/* MME signature (ascii "SMAP")  */

	.set	VGA_TEXT_COLOR_80x25,0x03

	.set	GDT_CODE64_SEL,0x08	/* Code64 selector */
	.set	GDT_CODE32_SEL,0x10	/* Code32 selector */
	.set	GDT_CODE16_SEL,0x18	/* Code16 selector */
	.set	GDT_DATA_SEL,0x20	/* Data selector */

	.set	KERNEL_STACK,0x7c00	/* Kernel stack */

	.set	KERNEL_SEG,0x1000	/* Memory where to load kernel */
	.set	KERNEL_OFF,0x0000	/*  segment and offset [1000:0000] */

	.file	"loader.s"

/* Text section */
	.text

	.globl	loader		/* Entry point */

	.code16			/* 16bit real mode */

/*
 * Kernel loader
 *   %cs:%ip=0x0900:0x0000 (=0x9000)
 *   %ss:%sp=0x0000:0x7c00 (=0x7c00)
 */
loader:
/* Save parameters */
	movw	%ss,stack16.ss
	movw	%sp,stack16.sp
	movw	%cs,code16.cs
/* Save descriptor table registers */
	sidt	idtr16
	sgdt	gdtr16

/* Enable A20 address line */
	call    enable_a20

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

/* Set VGA mode to 16bit color text mode */
	movb	$VGA_TEXT_COLOR_80x25,%al
	movb	$0x00,%ah
	int	$0x10

/* Turn on protected mode */
	cli
	lidt	idtr		/* Setupt temporary IDT */
	lgdt	gdtr		/* Setupt temporary GDT */
	movl	%cr0,%eax
	orb	$0x1,%al	/* Enable protected mode */
	movl	%eax,%cr0
	ljmp	$GDT_CODE32_SEL,$entry32	/* Go into protected mode */
						/*  and flush the pipline */


/* Enable A20 */
enable_a20:
	cli
	xorw	%cx,%cx		/* Zero */
enable_a20.1:
	incw	%cx		/* Increment, overflow? */
	jz	enable_a20.3	/* Yes, then give up */
	inb	$0x64,%al	/* Get status (0x64=status register) */
	testb	$0x2,%al	/* Busy? */
	jnz	enable_a20.1	/* Yes, then go back */
	movb	$0xd1,%al	/* Command: Write output port (0x60 to P2) */
	outb	%al,$0x64
enable_a20.2:
	inb	$0x64,%al	/* Get status */
	testb	$0x2,%al	/* Busy? */
	jnz	enable_a20.2	/* Yes, then go back */
	movb	$0xdf,%al	/* Enable A20 command */
	outb	%al,$0x60	/* Write to P2 via 0x60 output register */
enable_a20.3:
	sti			/* Enable interrupts */
	ret			/* Return to caller */


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
	testl	%ebx,%ebx		/* %ebx=0: No remaining info */
	jz	load_mm.done		/* jz/je */
load_mm.4:
	incw	%bp			/* Increment the number of entries */
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


/* Shutoff the machine using APM */
shutoff16:
/* Power off with APM */
	movw	$0x5301,%ax	/* Connect APM interface */
	movw	$0x0,%bx	/* Specify system BIOS */
	int	$0x15		/* Return error code in %ah with CF */

	movw	$0x530e,%ax	/* Set APM version */
	movw	$0x0,%bx	/* Specify system BIOS */
	movw	$0x102,%cx	/* Version 1.2 */
	int	$0x15		/* Return error code in %ah with CF */

	movw	$0x5308,%ax	/* Enable power management */
	movw	$0x1,%bx	/* All devices managed by the system BIOS */
	movw	$0x1,%cx	/* Enable */
	int	$0x15

	movw	$0x5307,%ax	/* Set power state */
	movw	$0x1,%bx	/* All devices managed by the system BIOS */
	movw	$0x3,%cx	/* Off */
	int	$0x15

	jmp	halt16


/* Reentry point for real mode to shutoff */
shutoff.reentry16:
/* Setup stack pointer */
	cli
	movw	stack16.ss,%ss
	movw	stack16.sp,%sp
	sti
	jmp	shutoff16


/* Halt (16bit mode) */
halt16:
	hlt
	jmp	halt16


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

	jmp	halt32


/* Halt (32bit mode) */
halt32:
	hlt
	jmp	halt32


/* Idle process */
idle:
	sti
	hlt
	cli
	jmp	idle


	.align	16
	.data
/* Data for reentry to real mode */
stack16.ss:
	.word	0x0
stack16.sp:
	.word	0x0
code16.cs:
	.word	0x0

/* Saved IDT register */
idtr16:
	.word	0
	.long	0
/* Saved GDT register */
gdtr16:
	.word	0
	.long	0


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

