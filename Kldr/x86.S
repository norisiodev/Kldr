/*
 * Implementtion of miscellaneous routines.
 *
 * Copyright (c) 2022 norisio.dev
 *
 * SPDX short identifier: MIT
 *
 */
.text

// rdi:1st argument, rsi:2nd argument

ASM_GLOBAL ASM_PFX(boot_lin64)
ASM_PFX(boot_lin64):

	cli

	movw	$0x18, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	movq	$0x10, %rax
	pushq	%rax
	pushq	%rdi

	retfq

	ret

ASM_GLOBAL ASM_PFX(chg_csds)
ASM_PFX(chg_csds):

	movw	%si, %ds
	movw	%si, %es
	movw	%si, %ss

	pushq	%rdi
	leaq	.ret(%rip), %rax
	pushq	%rax

	retfq
.ret:

	ret

ASM_GLOBAL ASM_PFX(sgdt)
ASM_PFX(sgdt):

	sgdt	(%rdi)

	ret

ASM_GLOBAL ASM_PFX(lgdt)
ASM_PFX(lgdt):

	lgdt	(%rdi)

	ret

ASM_GLOBAL ASM_PFX(getcs)
ASM_PFX(getcs):

	xorq	%rax, %rax
	movw	%cs, %ax

	ret

ASM_GLOBAL ASM_PFX(getds)
ASM_PFX(getds):

	xorq	%rax, %rax
	movw	%ds, %ax

	ret

ASM_GLOBAL ASM_PFX(getss)
ASM_PFX(getss):

	xorq	%rax, %rax
	movw	%ss, %ax

	ret
