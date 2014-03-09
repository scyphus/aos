/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.set	KERNEL_LBA,0x9		/* Kernel is located at LBA 9 */
	.set	KERNEL_SIZE,0x40	/* 64 sectors (32KiB) */
	/* Disk information */
	.set	HEAD_SIZE,18            /* 18 sectors per head/track */
	.set	CYLINDER_SIZE,2         /* 2 heads per cylinder */
	.set	NUM_RETRIES,3		/* Number of times to retry to read */
	.set	ERRCODE_TIMEOUT,0x80	/* Error code: timeout */

	/* Include other constant values */
	.include	"asmconst.h"

	.file		"loader.s"

	.org	0x0,0x0

/* Text section */
	.text

	.globl	bootmon		/* Entry point */

	.code16			/* 16bit real mode */

/*
 * Boot monitor
 *   %cs:%ip=0x0900:0x0000 (=0x9000)
 *   %ss:%sp=0x0000:0x7c00 (=0x7c00)
 *   %dl: drive
 */
bootmon:
/* Parameters from IPL */
	movb	%dl,drive
/* Save parameters */
	movw	%ss,stack16.ss
	movw	%sp,stack16.sp
	movw	%cs,code16.cs
/* Save descriptor table registers */
	sidt	idtr16
	sgdt	gdtr16

/* Enable A20 address line */
	call    enable_a20

/* Set VGA mode to 16bit color text mode */
	movb	$VGA_TEXT_COLOR_80x25,%al
	movb	$0x00,%ah
	int	$0x10

/* Disable interrupt */
	cli

	xorw	%ax,%ax
	movw	%ax,%es
	movw	$intr_int32,%ax
	movw	$(IVT_IRQ0),%bx
	call	setup_inthandler

	xorw	%ax,%ax
	movw	%ax,%es
	movw	$intr_int33,%ax
	movw	$(IVT_IRQ0+1),%bx
	call	setup_inthandler

/* Reset counter */
	movw	$1000,%ax
	movw	%ax,counter
	movw	$0,%ax
	movw	%ax,bootmode

/* Print out boot message */
	movw	$0,%ax
	movw	%ax,%ds
	movw	$msg_bootopt,%si
	call	putstr

/* Initialize PIT */
	call	init_pit

/* Wait for interaction */
wait:
	sti
	hlt
	cli
	movw	counter,%ax
	cmpw	$0,%ax
	jz	boot		/* Timeout then boot */
	movw	bootmode,%ax
	cmpw	$1,%ax
	je	boot		/* Pressed 1 (boot) */
	cmpw	$2,%ax
	je	shutoff16	/* Pressed 2 (power off) */
	jmp	wait

/* Boot */
boot:
	/* Check the CPU specification */
	movl	$1,%eax
	cpuid
	btl	$5,%edx		/* MSR support */
	jnc	cpuerror
	btl	$6,%edx		/* PAE */
	jnc	cpuerror
	btl	$9,%edx		/* Onboard APIC */
	jnc	cpuerror
	btl	$25,%edx	/* SSE */
	jnc	cpuerror
	btl	$26,%edx	/* SSE2 */
	jnc	cpuerror
	movl	$0x80000001,%eax
	cpuid
	btl	$29,%edx	/* Intel 64 support */
	jnc	cpuerror

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

	/* Load the kernel: Load 0x80 sectors (64KiB) from 0x1200 (LBA #9) */
	movb	drive,%dl
	testb	$0x80,%dl
	jz	rd.floopy
rd.hd:
	movw	$0x1000,%ax
	movw	%ax,%es
	movb	drive,%dl
	movb	$0x77,%al
	movb	$0x0,%ch
	movb	$0xa,%cl
	movb	$0x0,%dh
	movl	$0x0,%ebx
	movb	$0x02,%ah
	int	$0x13
	jc	read.fail	/* Fail (%cf=1) */
	cmpb	$0x77,%al
	jne	read.fail
	ljmp	$(KERNEL_SEG),$0

rd.floopy:
	movb	drive,%dl
	movw	$0x1000,%ax
	movw	%ax,%es
	movl	$0x0,%ebx
	movb	$0x1b,%dh
	movw	$9,%ax	/* from LBA 9 */
	call	read
	movb	drive,%dl
	movw	$0x1360,%ax
	movw	%ax,%es
	movl	$0x0,%ebx
	movb	$0x24,%dh
	movw	$0x24,%ax
	call	read
	movb	drive,%dl
	movw	$0x17e0,%ax
	movw	%ax,%es
	movl	$0x0,%ebx
	movb	$0x24,%dh
	movw	$0x48,%ax
	call	read
	movb	drive,%dl
	movw	$0x1c60,%ax
	movw	%ax,%es
	movl	$0x0,%ebx
	movb	$0x12,%dh
	movw	$0x6c,%ax
	call	read
	ljmp	$(KERNEL_SEG),$0

	movw	$KERNEL_SEG,%ax
	movw	%ax,%es
	movl	$(KERNEL_SEG<<4),%ebx
	movb	$KERNEL_SIZE,%dh
	movw	$KERNEL_LBA,%ax	/* from LBA 9 */
	call	read

	ljmp	$(KERNEL_SEG),$0


/* Display CPU error message */
cpuerror:
	movw	$0,%ax
	movw	%ax,%ds
	movw	$msg_cpuerror,%si
	call	putstr
	jmp	halt16

/* Initialize programmable interval timer  */
init_pit:
	pushw	%ax
	movb	$(0x00|0x30|0x06),%al
	outb	%al,$0x43
	movw	$0x2e9b,%ax	/* Frequency=100Hz: 1193181.67/100 */
	outb	%al,$0x40	/* Counter 0 */
	movb	%ah,%al
	outb	%al,$0x40	/* Counter 0 */
	popw	%ax
	ret

/*
 * Setup interrupt vector
 *   %es: code segment
 *   %ax: instruction pointer
 *   %bx: interrupt vector number
 */
setup_inthandler:
	pushw	%bx
	shlw	$2,%bx
	movw	%ax,(%bx)
	addw	$2,%bx
	movw	%es,(%bx)
	popw	%bx
	ret

/*
 * Timer interrupt handler
 */
intr_int32:
	pushw	%ax
	pushw	%dx
	pushw	%ds
	pushw	%si

	movw	counter,%ax
	testw	%ax,%ax
	jz	1f
	decw	%ax
1:
	movw	%ax,counter
	movb	$100,%dl
	divb	%dl		/* Q=%al, R=%ah */
	xorb	%ah,%ah
	movb	$10,%dl
	divb	%dl
	addb	$'0',%al
	addb	$'0',%ah
	movw	%ax,msg_count

	xorw	%ax,%ax
	movw	%ax,%ds
	movw	$msg_countdown,%si
	call	putbmsg

	/* EOI for PIC1 */
	movb	$0x20,%al
	outb	%al,$0x20

	popw	%si
	popw	%ds
	popw	%dx
	popw	%ax
	iret

/*
 * Keyboard interrupt handler
 */
intr_int33:
	pushw	%ax
	/* Scan code from keyboard controller */
	inb	$0x60,%al
	/* Check it is pressed or released? */
	testb	$0x80,%al
	jnz	4f		/* Jump if released */
	cmpb	$2,%al		/* '1' is pressed */
	jnz	1f
	/* Output '1' */
	movb	$'1',%al
	call	putc
	movw	$1,bootmode
	jmp	3f
1:
	cmpb	$3,%al		/* '2' is pressed */
	jnz	2f
	/* Output '2' */
	movb	$'2',%al
	call	putc
	movw	$2,bootmode
	jmp	3f
2:
	/* Output ' ' for other keys */
	movb	$' ',%al
	call	putc
	movw	$0,bootmode
3:
	/* Backspace */
	movb	$0x08,%al
	call	putc
4:
	/* EOI for PIC1 */
	movb	$0x20,%al
	outb	%al,$0x20
	popw	%ax
	iret



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


/* Display a null-terminated string */
putstr:
putstr.load:
	lodsb			/* Load %al from %ds:(%si), then incl %si */
	testb	%al,%al		/* Stop at null */
	jnz	putstr.putc	/* Call the function to output %al */
	ret			/* Return if null is reached */
putstr.putc:
	call	putc		/* Output a character %al */
	jmp	putstr.load	/* Go to next character */
putc:
	pushw	%bx		/* Save %bx */
	movw	$0x7,%bx	/* %bh: Page number for text mode */
				/* %bl: Color code for graphics mode */
	movb	$0xe,%ah	/* BIOS: Put char in tty mode */
	int	$0x10		/* Call BIOS, print a character in %al */
	popw	%bx		/* Restore %bx */
	ret


/* Read %dh sectors starting at LBA (logical block address) %ax on drive %dl
   into %es:[%bx] */
read:
	pushw	%bp		/* Save the base pointer */
	movw	%sp,%bp		/* Copy the stack pointer to the base pointer */
/* Save registers */
	movw	%ax,-2(%bp)
	movw	%bx,-4(%bp)
	movw	%cx,-6(%bp)
	movw	%dx,-8(%bp)
	/* u16 track:sector -10(%bp) */
	/* u16 counter -12(%bp) */
	subw	$12,%sp

/* Convert LBA to CHS */
	call	lba2chs		/* Convert LBA %ax to CHS (%ch,%dh,%cl) */
	movw	%cx,-10(%bp)	/* Save %cx to the stack */
/* Restore %bx */
	movw	-4(%bp),%bx
/* Reset counter */
	xorw	%cx,%cx
	movw	%cx,-12(%bp)
read.retry:
	movw	-8(%bp),%ax	/* Get the saved %dx from the stack */
	movb	%ah,%al		/*  (Note: same as movb -7(%bp),%al */
	movb	$0x02,%ah       /* BIOS: Read sectors from drive */
	movw	-10(%bp),%cx	/* Get the saved %cx from the stack */
	int	$0x13		/* CHS=%ch,%dh,%cl, drive=%dl, count=%al */
				/*  to %es:[%bx] (results in %ax,%cf) */
	jc	read.fail	/* Fail (%cf=1) */

/* Restore registers */
	movw	-8(%bp),%dx
	movw	-6(%bp),%cx
	movw	-4(%bp),%bx
	movw	-2(%bp),%ax
	movw	%bp,%sp		/* Restore the stack pointer and base pointer */
	popw	%bp
	ret
read.fail:
	movw	-12(%bp),%cx
	incw	%cx
	movw	%cx,-12(%bp)
	cmpw	$NUM_RETRIES,%cx
	ja	read.error		/* Exceed retries */
	cmpb	$ERRCODE_TIMEOUT,%ah	/* Timeout? */
	je	read.retry		/* Yes, then retry */
read.error:				/* We do not restore the stack */
	movb	%ah,%al			/* Save error */
	movw	$hex_error,%di		/* Format it as hex */
	xorw	%bx,%bx
	movw	%bx,%es
	call	hex8
	movw	$msg_readerror,%si	/* Display the read error message */
	jmp	readerror

/* LBA: %ax into CHS %ch,%dh,%cl */
lba2chs:
	pushw	%bx		/* Save */
	pushw	%dx
/* Compute sector */
	xorw	%dx,%dx
	movw	$HEAD_SIZE,%bx
	mov	%eax,%dr0
	divw	%bx		/* %dx:%ax / %bx; %ax:quotient, %dx:remainder */
	incb	%dl
	movb	%dl,%cl		/* Sector */
/* Compute head and track */
	xorw	%dx,%dx
	movw	$CYLINDER_SIZE,%bx
	divw	%bx		/* %dx:%ax / %bx */
	movw	%dx,%bx		/* Save the remainder to %bx */
	popw	%dx		/* Restore %dx*/
	movb	%bl,%dh		/* Head */
	movb	%al,%ch		/* Track */
	popw	%bx		/* Restore %bx */
	ret

/* Display error message at %si and then halt. */
readerror:
	call	putstr			/* Display error message */
	jmp	halt16

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


/*
 * Bottom-line message
 *   %ds:%si --> 0xb800:**
 */
putbmsg:
	pushw	%ax
	pushw	%es
	pushw	%di
	movw	$0xb800,%ax
	movw	%ax,%es
	movw	$(80*24*2),%di
putbmsg.load:
	lodsb			/* Load %al from %ds:(%si) , then incl %si */
	testb	%al,%al		/* Stop at null */
	jnz	putbmsg.putc	/* Call the function to output %al */
	popw	%di
	popw	%es
	popw	%ax
	ret
putbmsg.putc:
	movb	$0x7,%ah
	stosw
	jmp	putbmsg.load



/* Data section */
	.align	16
	.data
/* Counter for boot monitor */
counter:
	.word	0x0
/* Boot mode */
bootmode:
	.word	0x0

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


/* Saved boot drive */
drive:
	.byte	0

/* Messages */
msg_bootopt:
	.ascii	"Select one:\r\n"
	.ascii	"    1: Boot (64 bit mode)\r\n"
	.ascii	"    2: Power off\r\n"
	.asciz	"Press key:[ ]\x08\x08"

msg_cpuerror:
	.asciz	"\r\n\nUnsupported CPU.\r\n\n"

msg_countdown:
	.ascii	"AOS will boot in "
msg_count:
	.asciz	"00 sec."

msg_readerror:
	.ascii  "\r\n\nRead Error: 0x"
hex_error:
	.asciz  "00\r\r"
