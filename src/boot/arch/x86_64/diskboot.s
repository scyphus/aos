/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.file	"diskboot.s"

/* Text section */
	.text

	.code16			/* 16bit real mode */
	.globl	start		/* Entry point */

start:
/* Setup the segment registers for flag addressing and setup the stack. */
	cld			/* Clear direction flag */
				/* (inc di/si for str ops) */
	xorw	%ax,%ax		/* Zero %ax */
/* Setup stack */
	movw	%ax,%ss
	movw	$start,%sp
/* Setup data segments */
	movw	%ax,%ds
	movw	%ax,%es
/* Save BIOS boot device */
	movb	%dl,drive
/* Display the welcome message */
	movw	$msg_welcome,%si	/* %ds:(%si) -> welcome message */
	call	putstr

/* Halt */
halt:
	hlt
	jmp	halt

/* Display a null-terminated string */
putstr:
	pushw	%bx
putstr.load:
	lodsb			/* Load %al from %ds:(%si), then incl %si */
	testb	%al,%al		/* Stop at null: could be orb */
	jnz	putstr.putc	/* Output a character*/
	popw	%bx
	ret			/* return */
putstr.putc:
	call	putc		/* Output a character */
	jmp	putstr.load	/* Go to next character */
putc:
	movw	$0x7,%bx	/* Color code */
	movb	$0xe,%ah	/* BIOS: put_char */
	int	$0x10		/* call BIOS, print a character in %al */
	ret


/* Data section */
	.data

/* Saved boot drive */
drive:
	.byte	0

/* Messages */
msg_welcome:
	.asciz	"Welcome!\r\n\nLet's get it started.\r\n\n"

/*
 * Local variables:
 * tab-width: 8
 * c-basic-offset: 8
 * End:
 * vim600: sw=8 ts=8 fdm=marker
 * vim<600: sw=8 ts=8
 */
