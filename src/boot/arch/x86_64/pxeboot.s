
	.set	PXE_SIGNATURE,0x45585021
	.set	KERNEL_ADDR,0x10000

	.text
	.code16
	.globl	pxeboot

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
	movl	%eax,%dr0
	testw	%ax,%ax
	jnz	error
	/* Check the packet number */
	incw	%cx
	cmpw	t_PXENV_TFTP_READ.PacketNumber,%cx
	jne	error
	/* Check the size */
	xorl	%eax,%eax
	movw	t_PXENV_TFTP_READ.BufferSize,%ax
	addl	%edx,%eax
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


	ljmp	$0x1000,$(halt-0x7c00)

	jmp	halt


pxeapi:
	push	%ds
	push	%di
	push	%bx
	lcall	*(pm_entry)
	addw	$6,%sp
	ret


error:
	jmp	halt

halt:
	hlt
	jmp	halt



/* Data section */
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



/* Second stage file */
ssfile:
	.asciz	"pxepix"


