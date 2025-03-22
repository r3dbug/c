
	XDEF _CalcRiced
	XDEF _add64
	XDEF _lsr64
	XDEF _lsl64
	XDEF _add6464
	XDEF _cpy6464
	XDEF _mul323264
	XDEF _mul646432
	
_lsr64:
; a0: long64
; d1: shift
	store d0,-(sp)
	load  (a0),d0
	lsrq  d1,d0,d0
	store d0,(a0)
	load (sp)+,d0
	rts

_lsl64:
; a0: long64
; d1: shift
	store d0,-(sp)
	load  (a0),d0
	lslq  d1,d0,d0
	store d0,(a0)
	load (sp)+,d0
	rts
	
_add64:
; a0: long64
; d0: op2
	movem.l d1-d2,-(sp)
	move.l  (a0)+,d1
	move.l  (a0)+,d2
	add.l	d0,d2
	bcc     .nocarry
	add.l   #1,d1
.nocarry
	move.l  d2,-(a0)
	move.l  d1,-(a0)
	movem.l (sp)+,d1-d2
	rts

_cpy6464:
	store	d0,-(sp)
	load	(a1),d0
	store	d0,(a0)	
	load    (sp)+,d0
	rts
	
_add6464:
; a0/a1: op1(dest)/2
	movem.l	d0-d3,-(sp)
	move.l  (a0)+,d0
	move.l  (a0)+,d1
	move.l  (a1)+,d2
	move.l  (a1)+,d3
	add.l   d3,d1
	bcc		.dcc
	add.l	#1,d0
	
.dcc:
	add.l   d2,d0
	movem.l	d1,-(a0)
	movem.l d0,-(a0)
	
	movem.l (sp)+,d0-d3
	rts

_mul323264:
; a0: long64 (dest)
; d0/d1: op1/2 (src)
	movem.l d2,-(sp)
	muls.l  d0,d2:d1		; hi/lo	
	move.l	d2,(a0)+
	move.l  d1,(a0)
	movem.l (sp)+,d2
	rts
	
_mul646432:
; a0: long64 (dest)
; a1: long64 (src1)
; d0: long (src)
	movem.l d1-d7,-(sp)
	move.l  (a1)+,d3		; hi (a1)
	move.l  (a1)+,d1		; lo (a1)
	move.l  d3,d5
	muls.l  d0,d2:d1		; hi/lo	
	
	;move.l	d2,d4			; hi
	;move.l  d1,d5			; lo
	;muls.l  d3,d5			; hi(a1) x lo
	
	cmp.l   #$ffffffff,d3
	beq.s   .nohimul
	muls.l	d0,d3			; lo (d0) x hi (a1)
	add.l   d3,d2
.nohimul:	
	move.l	d2,(a0)+
	move.l  d1,(a0)
	movem.l (sp)+,d1-d7
	rts
		
_CalcRiced:
; d0: param
; d1: val
; d2: t
	store 	d3,-(sp)
	lslq  	d0,d1,d3		; val << param  
	or.l    d2,d3			; val1 = (val << param) | t
	
	move.l  d3,d1			; lower part of val1
	and.l   #1,d1
	neg.l   d1				; -(val & 1)
	
	lsrq	#1,d3,d3		; val1 >> 1

	eor.l   d3,d1
	move.l  d1,d0
	load 	(sp)+,d3
	rts