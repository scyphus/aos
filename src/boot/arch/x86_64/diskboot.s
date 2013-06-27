
/* Buffer to read from floppy disk */
	.set	ERROR_TIMEOUT,0x80	/* BIOS timeout on read */
	.set	NUM_RETRIES,3		/* Number of times to retry */
	.set	MEM_READ_BUFFER,0x9000
/* Floppy disk */
	.set	SECTOR_SIZE,0x200	/* 0x200 for FD, 0x800 for CD */
	.set	HEAD_SIZE,18		/* 18 sectors per head/track */
	.set	CYLINDER_SIZE,2		/* 2 heads per cylinder */
	.set	NUM_CYLINDER,80		/* 80 cylinders in FD */

	.file	"diskboot.s"
	.text

	.code16
	.globl	start		/* Entry point */

	.org	0x0,0x0

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

/* Load the kernel */
load_kernel:
	//movw	$0x21,datasector

/* Load 0x20 sector from 1 */
	movw	$MEM_READ_BUFFER,%bx
	movb	$0x20,%dh
	movw	$0x1,%ax

	pushw	%ax
	call	read
	popw	%ax

	jmp	$0,$MEM_READ_BUFFER

/* Read %dh sectors starting at LBA (logical block address) %ax into [%bx] */
read:
	pushw	%bx
	pushw	%cx
	pushw	%dx

	pushw	%ax
	movw	%bx,%ax
	andw	$0xf,%bx
	shrw	$4,%ax
	movw	%ax,%es
	popw	%ax

	pushw	%bx
	xorw	%cx,%cx
	movw	%dx,%bx
	call	lba2chs
	movb	%bh,%al
	popw	%bx
read.retry:
	call	twiddle		/* Entertain the user */
	pushw	%ax
	pushw	%dx
	movb	drive,%dl	/* BIOS device */
	movb	$0x02,%ah	/* BIOS: Read sectors from drive */
	int	$0x13
	popw	%dx		/* Restore */
	popw	%ax
	jc	read.fail	/* Fail (%cf=1) */
	popw	%dx		/* Restore */
	popw	%cx
	popw	%bx
	ret
read.fail:
	incw	%cx
	cmpw	%cx,NUM_RETRIES
	ja	read.error		/* Exceed retries */
	cmpb	$ERROR_TIMEOUT,%ah	/* Timeout? */
	je	read.retry		/* Yes, then retry */
read.error:
	movb	%ah,%al			/* Save error */
	movw	$hex_error,%di		/* Format it as hex */
	call	hex8
	movw	$msg_badread,%si	/* Display the read error message */
	jmp	error

/* LBA: %ax into CHS %ch,%dh,%cl */
lba2chs:
	pushw	%bx
	xorw	%dx,%dx
	movw	$HEAD_SIZE,%bx
	divw	%bx		/* %ax mod %dx = %dx:%ax / %bx */
	incb	%dl
	movb	%dl,%cl		/* Sector */
	xorw	%dx,%dx
	movw	$CYLINDER_SIZE,%bx
	divw	%bx
	movb	%dl,%dh		/* Head */
	movb	%al,%ch		/* Track */
	popw	%bx
	ret

/* Read %dh sectors starting at LBA (logical block address) %eax into [%ebx] */
readlba:
	pushw	%si
	pushw	%cx		/* Save since some BIOSs trash */
	movl	%eax,edd_lba	/* LBA to read from */
/* Convert address to segment, and store */
	movl	%ebx,%eax
	shrl	$4,%eax
	movw	%ax,edd_addr+0x2
readlba.retry:
	call	twiddle		/* Entertain the user */
	pushw	%dx
	movw	$edd_packet,%si	/* Address packet */
	movb	%dh,edd_len	/* Set length */
	movb	drive,%dl	/* BIOS Device */
	movb	$0x42,%ah	/* BIOS: Extended read */
	int	$0x13		/* Call BIOS */
	popw	%dx		/* Restore */
	jc	readlba.fail	/* Fail (%cf=1) */
	popw	%cx		/* Restore */
	popw	%si		/* Restore  */
	ret
readlba.fail:
	cmpb	$ERROR_TIMEOUT,%ah	/* Timeout? */
	je	readlba.retry		/* Yes, then retry */
readlba.error:
	movb	%ah,%al			/* Save error */
	movw	$hex_error,%di		/* Format it as hex */
	call	hex8
	movw	$msg_badread,%si	/* Display the read error message */

/* Display error message at %si and then halt. */
error:
	call	putstr		/* Display message */
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



twiddle:
	pushw	%ax
	pushw	%bx
	movb	twiddle_index,%al	/* Load index */
	movw	$twiddle_chars,%bx	/* Address table */
/* Next char*/
	incb	%al
	andb	$3,%al
	movb	%al,twiddle_index	/* Save index for next call */
	xlatb				/* Get char (%al+%bx) -> %al */
	call	putc			/* Output it */
	movb	$8,%al			/* Backspace */
	call	putc			/* Output it */
	popw	%bx
	popw	%ax
	ret


/* Convert AL to hex char, saving the result to [EDI]. */
hex8:
	pushl	%eax
/* Do upper 4 */
	shrb	$0x4,%al
	call	hex8.1
	popl	%eax
hex8.1:
	andb	$0xf,%al	/* Get lower 4 */
	cmpb	$0xa,%al	/* Convert  : CF=1 if %al < $0xa */
	sbbb	$0x69,%al	/*  to hex  : %al<-%al - $0x69 - CF */
	das			/*  digit   : BCD */
	orb	$0x20,%al	/* To lower case */
	stosb			/* Save char to %di and inc %di */
	ret			/* (Recursive) */


/* For data */
	.data

/* Enhanced Disk Drive (EDD) Packet (disk address packet) */
edd_packet:
	.byte	0x10		/* Length of DAP */
	.byte	0		/* Unused */
edd_len:
	.byte	0x0		/* Number of sectors to read */
	.byte	0		/* Reserved */
edd_addr:
	.word	0x0,0x0		/* Segment:Offset pointer to memory buffer */
edd_lba:
	.quad	0x0		/* LBA (first sector = 0) */


/* Saved drive */
drive:
	.byte	0

/* State for searching */
twiddle_index:
	.byte	0x0

/* Messages */
msg_welcome:
	.asciz	"Scyphus Boot Loader 1.0\r\n\n"
msg_badread:
	.ascii	"Read Error: 0x"
hex_error:
	.asciz	"00\r\r"
twiddle_chars:
	.ascii	"|/-\\"

/*
 * Local variables:
 * tab-width: 8
 * c-basic-offset: 8
 * End:
 * vim600: sw=8 ts=8 fdm=marker
 * vim<600: sw=8 ts=8
 */
