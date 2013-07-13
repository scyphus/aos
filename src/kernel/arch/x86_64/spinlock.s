/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.file	"spinlock.s"

	.globl	_spin_lock
	.globl	_spin_unlock

/* void spin_lock(u32 *); */
_spin_lock:
	movl	$1,%eax
1:	xchgl	(%rdi),%eax
	testl	%eax,%eax
	jnz	1b
	ret

/* void spin_unlock(u32 *); */
_spin_unlock:
	xorl	%eax,%eax
	xchgl	(%rdi),%eax
	ret
