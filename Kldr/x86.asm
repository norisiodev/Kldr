/*
 * The application to load the linux kernel and initrd.
 * 
 * Copyright (c) 2022 norisio.dev
 * 
 * SPDX short identifier: MIT
 * 
 */

.code

boot_lin64 PROC
	cli

	mov	rsi, rdx

	mov	ax, 018h
	mov	ds, ax
	mov	es, ax
	mov	ss, ax

	mov	rax, 010h
	push	rax
	push	rcx

	db	048h	; rex.w
	retf

	ret
boot_lin64 ENDP

chg_csds PROC
	mov	ds, dx
	mov	es, dx
	mov	ss, dx

	push	rcx
	lea	rax, segjmp
	push	rax

	db	048h	; rex.w
	retf
segjmp:

	ret
chg_csds ENDP

_sgdt PROC
	sgdt	fword ptr [rcx]
	ret
_sgdt ENDP

_lgdt PROC
	lgdt	fword ptr [rcx]
	ret
_lgdt ENDP

getcs PROC
	xor	rax, rax
	mov	ax, cs

	ret
getcs ENDP

getds PROC
	xor	rax, rax
	mov	ax, ds

	ret
getds ENDP

getss PROC
	xor	rax, rax
	mov	ax, ss

	ret
getss ENDP

	END
