
	.text
	.code16
	.globl	pxeboot

pxeboot:
	cld

	cli
	xorw	%ax,%ax
/* Reset %ds */
	movw	%ax,%ds
/* Save */
	movw	%es,pxenv_seg
	movw	%bx,pxenv_off
	//movw	%ss,pxeopt_seg
	//movw	%sp,pxeopt_off
	movw	%ss,%ax
	movw	%ax,%es
	movw	%sp,%bx
	addw	$4,%bx
	movl	%es:(%bx),%eax
	movw	%ax,pxe_off
	shrl	$16,%eax
	movw	%ax,pxe_seg
	sti

	movw	pxe_seg,%ax
	movw	%ax,%es
	movw	pxe_off,%bx
	movl	%es:(%bx),%eax
	movl	%eax,%dr0

1:
	hlt
	jmp	1b



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
//pxeopt_seg:
//	.word	0x0
//pxeopt_off:
//	.word	0x0

