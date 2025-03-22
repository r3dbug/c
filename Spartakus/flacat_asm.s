

	XDEF _add6464
	XDEF _mul646432
	XDEF _lsr64
	XDEF _lsl64
	XDEF _LE2BE32
	XDEF _LE2BE16
;	XDEF _LE2BE32BUFFER
;	XDEF _playstereo
;	XDEF _stopplaying
;	XDEF _mousedown
	XDEF _get_bits_asm
	XDEF _SetVampireTaskSwitching
;	XDEF _get_rice_asm
	XDEF _count_zeros
	XDEF _calc_param
		
_SetVampireTaskSwitching:
; set bit 11 ("Apollo bit") in SR in order to signal
; exec that ALL register (= "classic" ones plus new
; b0-b7 and e0-e23 registers) should be saved during
; a task switch
    ori     #%100000000000,SR
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
	
_mul646432:
; a0: long64 (dest)
; a1: long64 (src1)
; d0: long (src)
	movem.l d1-d7,-(sp)
	move.l  (a1)+,d3		; hi (a1)
	move.l  (a1)+,d1		; lo (a1)
	move.l  d3,d5
	muls.l  d0,d2:d1		; hi/lo	
	
	cmp.l   #$ffffffff,d3
	beq.s   .nohimul
	muls.l	d0,d3			; lo (d0) x hi (a1)
	add.l   d3,d2

.nohimul:	
	move.l	d2,(a0)+
	move.l  d1,(a0)
	movem.l (sp)+,d1-d7
	rts
	
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
	
_LE2BE32:
; d0.l: value
	movex.l d0,d0
	rts
	
_LE2BE16
; d0.w: value
	movex.l	d0,d0
	swap	d0
	rts	

;_LE2BE32BUFFER
; a0: data
; d0: counter
;	movem.l		d1,-(sp)
;	bra.s		.d
;.l
;	move.l		(a0),d1
;	movex.l		d1,(a0)+
;.d
;	dbra.l		d0,.l
;	movem.l     (sp)+,d1
;	rts

;_playstereo:
; a0: buffer
; d0: length
; d1: volume
; d2: channel
; d5: mode
; d6: period (frequency)
; returns: 
; channel in d0 
; (0xff if no channel available)
;	movem.l		d3-d4/a1,-(sp)
;	; bra.b		.free
	
;	move.l      #$dff400,a1							; ARNE channel 0
;	and.l		#$f,d2								; 16 channels
;	move.l		d2,d4
;	move.l		d2,d3
	
;	lsl.l		#4,d4
;	add.l       d4,a1								; ARNE channel x
	
;	moveq       #1,d4
;	sub.l       #4,d3								; bit position of channel x
;	lsl.l		d3,d4								; bit mask for channel x
	
;	move.l		d4,d3
;	and.w	    $dff202,d3							; channel x free?
;	beq.b		.free
	
;	move.l		#$ff,d0								; not free
;	bra.b		.end
	
;.free
;	move.l      a0,(a1)                       		; pcm data ptr	
;	lsr.l		#1,d0
;    move.l     	d0,$4(a1) 							; length
;    move.w      d1,$8(a1) 							; volume
;	move.w      d6,$c(a1) 							; samplerate (44khz)
;    move.w      d5,$a(a1) 							; ARNE mode (e.g. 16bit stereo, loop) 
;	or.l		#$8000,d4
;	move.w		d4,$dff296
;	move.l		d2,d0								; return request channel (which is free)
;.end
;	movem.l		(sp)+,d3-d4/a1
;	rts

;_stopplaying:
;; d0: channel
;	moveq		#1,d1
;	and.l		#$f,d0							; 16 channels
;	sub.l		#4,d0							; bit 0 = channel 4
;	lsl.l		d0,d1							; bit mask channel x
;	; move.w      #$0008,$dff296						; DMACON2 for channel 7 (= bit 3)
;	;move.w		#$f,$dff296 ; turn all off
;	move.w      d1,$dff296						; DMACON2 for channel 7 (= bit 3)
;	rts

;_mousedown:
;	clr.l		d0
;	btst		#6,$bfe001
;	bne.b		.no
;	st			d0
;.no
;	rts

_get_bits_asm:
; a0: ptr stream_buf	; ptr flacd
; a1: ptr bit_pos
; d0: bits (int)
; d1: sign (int)
; return: unsigned int
;---------------------
	movem.l		d2-d5,-(sp)

	move.l		(a1),d2
	lsr.l       #3,d2			; byte position
	
	load		(a0,d2.l),d3
	
	move.l		(a1),d4			
		add.l		d0,(a1)
	and.l       #7,d4			; bit position
		move.l		#32,d5
		
	;add.l		d0,(a1)
	
	lslq		d4,d3,d3

	; lsrq		#32,d3,d3
	
	;move.l		#32,d5
	sub.l		d0,d5
	
	lsrq		#32,d3,d3
	
	
	tst.l		d1
	beq.s		.nosign
	
	asr.l		d5,d3
	bra			.cont
	
.nosign:
	lsr.l		d5,d3

.cont:
	move.l		d3,d0
	
.final:		
	movem.l		(sp)+,d2-d5
	rts

_count_zeros:
;_get_rice_asm:
; a0: ptr flacd
	movem.l		d1-d7/a1,-(sp)
	
	move.l		(a0),a1				; ptr stream_buf
	move.l      4(a0),d1			; ptr bit_pos

	move.l		d1,d2
	lsr.l		#3,d2				; byte position
	and.l		#7,d1				; bit position
	
	load		(a1,d2.l),d3
	
	lslq		d1,d3,d3
	lsrq		#32,d3,d3
	
	;comment in to see raw value used to calculate r
	;move.l      d3,d0
	;bra .out
	
	clr.l		d0
	move.l		#$80000000,d5
	
.loop
	move.l		d5,d6
	and.l		d3,d6
	
	cmp.l		d5,d6
	beq.s		.out
	
	add.l		#1,d0
	lsr.l		#1,d5
	bra.s		.loop
	
.out
	add.l		d0,4(a0)
	add.l		#1,4(a0)
	
	movem.l		(sp)+,d1-d7/a1
	rts
	
_calc_param:
; d1: r (ULONG)
; d2: param (LONG)
; d3: data (ULONG)
; returns: d0
	move.l		d1,d0		; r
	lsl.l		d2,d0
	or.l		d3,d0		; r' = (r<<param) | get_uint(param);
	
	move.l		d0,d1
	
	lsr.l		#1,d0		; (r'>>1)
	
	and.l		#1,d1
	neg.l		d1			; -(r'&1)
	
	eor.l		d1,d0		; (r'>>1) ^ (-(r'&1));
	rts
