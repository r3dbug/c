
	XDEF	_playstereo
	XDEF	_waitmouse
	XDEF	_stopplaying
	XDEF	_mousedown
	XDEF	_detectvamp
	XDEF 	_BUFFER_LE2BE_LONG
	XDEF	_LE2BE_LA
	XDEF	_LE2BE_WA
	
	include "includes/dos_lib.i"
	include "includes/exec_lib.i"
		
CALLDOS2	MACRO
		    move.l	    dosbase,a6
		    jsr	        _LVO\1(a6)
		    ENDM       

VBLDELAY	MACRO
		    movem.l	    d0-a6,-(a7)
		    move.l	    #\1,d1
		    CALLDOS2	Delay
		    movem.l	    (a7)+,d0-a6
		    ENDM

_playstereo:
; a0: buffer
; d0: length
; d1: volume
; d2: channel
; d5: mode
; d6: period (frequency)
; returns: 
; channel in d0 
; (0xff if no channel available)
	movem.l		d3-d4/a1,-(sp)
	; bra.b		.free
	
	move.l      #$dff400,a1							; ARNE channel 0
	and.l		#$f,d2								; 16 channels
	move.l		d2,d4
	move.l		d2,d3
	
	lsl.l		#4,d4
	add.l       d4,a1								; ARNE channel x
	
	moveq       #1,d4
	sub.l       #4,d3								; bit position of channel x
	lsl.l		d3,d4								; bit mask for channel x
	
	move.l		d4,d3
	and.w	    $dff202,d3							; channel x free?
	beq.b		.free
	
	move.l		#$ff,d0								; not free
	bra.b		.end
	
.free
	move.l      a0,(a1)                       		; pcm data ptr	
	lsr.l		#1,d0
    move.l     	d0,$4(a1) 							; length
    move.w      d1,$8(a1) 							; volume
	move.w      d6,$c(a1) 							; samplerate (44khz)
    move.w      d5,$a(a1) 							; ARNE mode (e.g. 16bit stereo, loop) 
	or.l		#$8000,d4
	move.w		d4,$dff296
	move.l		d2,d0								; return request channel (which is free)
.end
	movem.l		(sp)+,d3-d4/a1
	rts

_stopplaying:
; d0: channel
	moveq		#1,d1
	and.l		#$f,d0							; 16 channels
	sub.l		#4,d0							; bit 0 = channel 4
	lsl.l		d0,d1							; bit mask channel x
	; move.w      #$0008,$dff296						; DMACON2 for channel 7 (= bit 3)
	;move.w		#$f,$dff296 ; turn all off
	move.w      d1,$dff296						; DMACON2 for channel 7 (= bit 3)
	rts

_BUFFER_LE2BE_LONG
; a0: data
; d0: counter
	movem.l		d1,-(sp)
	bra.s		.d
.l
	move.l		(a0),d1
	swap		d1
	movex.l		d1,(a0)+
.d
	dbra.l		d0,.l
	movem.l     (sp)+,d1
	rts
	
_BUFFER_LE2BE_LONG_AMMX 		; not faster
; a0: data
; d0: counter
	movem.l		d1,-(sp)
	btst		#0,d1
	bne.s		.even
	movex.l		(a0),d1
	move.l		d1,(a0)+
.even
	lsr.l		#1,d1
	bra.s		.d
.l
	load		(a0),d1
	vperm       #$32107654,d1,d1,d1
	store		d1,(a0)+
.d
	dbra.l		d0,.l
	movem.l     (sp)+,d1
	rts

_LE2BE_LA						; LE2BE LONG ASM
; d0: data
	movex.l		d0,d0
	rts	

_LE2BE_WA						; LE2BE WORD ASM
; d0: data
	movex.l		d0,d0
	swap		d0
	rts	
			
_waitmouse:
	btst		#6,$bfe001
	bne			_waitmouse
	rts

_mousedown:
	clr.l		d0
	btst		#6,$bfe001
	bne.b		.no
	st			d0
.no
	rts

_detectvamp:
    move.l   $4.w,a0
    move.w   $128(a0),d0    	; ExecBase->AttnFlags
    
    ; check for Vampire
    btst.l   #10,d0         	; AFB_68080 = bit 10
    beq.s    .no68080
    
.is68080:
    clr.l	 d0
    move.w   $dff3fc,d0
    lsr.l    #8,d0
    rts
    
.no68080:
    clr.l    d0
    rts  

_detectm68k:
    move.l   $4.w,a0
    move.w   $128(a0),d0    	; ExecBase->AttnFlags

	btst     #7,d0				; 68060?
    beq      .68060	
    btst     #3,d0				; 68040?
    beq      .68040		  
    btst     #2,d0				; 68030?
    beq      .68030		  
    btst     #1,d0				; 68020?
    beq      .68020		  
    
	clr.l	 d0
    rts
    
.68020:
	move.l   #68020,d0
	rts
    
.68030:
	move.l   #68030,d0
	rts
    
.68040:
	move.l   #68040,d0
	rts
    
.68060:
	move.l   #68040,d0
	rts

_getattnflags:
	clr.l    d0
    move.l   $4.w,a0
    move.w   $128(a0),d0    	; ExecBase->AttnFlags
	rts
	     
    SECTION DATA

dosbase         dc.l    0
dosname         dc.b    "dos.library",0

    END