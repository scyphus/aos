/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

	.file	"spinlock.s"

	.globl	_spin_lock_intr
	.globl	_spin_unlock_intr
	.globl	_spin_lock
	.globl	_spin_unlock

/* void spin_lock_intr(u32 *); */
_spin_lock_intr:
	cli
/* void spin_lock(u32 *); */
_spin_lock:
	xorl	%ecx,%ecx
	incl	%ecx
1:
	xorl	%eax,%eax
	lock cmpxchgl	%ecx,(%rdi)
	jnz	1b
	ret

/* void spin_unlock(u32 *); */
_spin_unlock:
	xorl	%eax,%eax
	lock xchgl	(%rdi),%eax
	ret

/* void spin_unlock_intr(u32 *); */
_spin_unlock_intr:
	xorl	%eax,%eax
	lock xchgl	(%rdi),%eax
	sti
	ret
