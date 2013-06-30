/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.set	KERNEL_SEG,0x0900	/* Memory where to load kernel loader */
	.set	KERNEL_OFF,0x0000	/*  segment and offset [0900:0000] */
	.set	KERNEL_SIZE,0x8		/* Size of kernel loader in sectors */

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
/* Enable A20 address line */
	call    enable_a20
	jmp	shutoff16

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

/* Came into the real mode then immediately shutoff */
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

	jmp	halt16		/* For the case of failure */

/* Halt (16bit mode) */
halt16:
	hlt
	jmp	halt16


