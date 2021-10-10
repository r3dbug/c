
	section fractal,code
	
	xref _pokemode
	xref _restoremode
	xref _fillscreen
	xref _waitmouse
	xref _setcolor
	xref _setstart
	xref _setstop
	xref _getstart
	xref _getstop
	xref _calcpoint
	
_pokemode:
	; res(d0) + form(d1) => modeword
	lsl.w   #8,d0
	add.w   d1,d0
	move.w  d0,modeword
	
	; save old system values
	move.w	$dff002,d0			; save dmacon
	or.w	d0,dmacon
	move.w	#$7fff,$dff096		; all dma off

	move.w	$dff01c,d0			; save intena
	or.w	d0,intena
	move.w	#$7fff,$dff09a		; all intena off

	move.l	$dff1ec,oldptr	; save gfxptr	
	move.w	$dff1f4,oldmode		; save oldmode

	; prepare new mode
	clr.w	$dff1e6				; clear modulo
	move.l	a0,d0				; buffer (a0)
	addi.l  #31,d0				; align to avoid off-by-one effects
	andi.l  #~31,d0
	move.l	d0,aligned
	
	; set new mode
	move.l	d0,$dff1ec				; returns aligned buffer in d0
	move.w	modeword,$dff1f4		; set gfx 640x360 8bit

    rts
	
_restoremode:
	move.w	#$7fff,$dff096
	move.w	dmacon,$dff096
	move.w	intena,$dff09a
	move.l	oldptr,$dff1ec
	move.w	oldmode,$dff1f4

	move.l	iteration,d0
	rts

_fillscreen:
	move.l aligned,a0
	move.w d1,tempy
	sub.w  #2,d0
.ny:
	move.w tempy,d1
	sub.w  #2,d1
.nx:
	move.b  d2,(a0)+
	add.w   #1,d4
	and.w   #%111111,d4
	dbra d1,.nx
	dbra d0,.ny
	rts
		
_waitmouse:
	btst	#6,$bfe001
	bne.s   _waitmouse
	rts
	
_setcolor:
    and.l   #$ff,d0
	and.l   #$ff,d1
	and.l   #$ff,d2
	and.l   #$ff,d3
	
	lsl.l 	#8,d0
	add.l   d1,d0
	swap 	d0
	lsl.l   #8,d2
	add.l   d2,d0
	add.l   d3,d0
	move.l	d0,$dff388
	rts

_setstart:
	dc.w    $4e7a,$0809			; movec   $0809,d0
	move.l  d0,time_start
	rts
	
_setstop:
	dc.w    $4e7a,$0809			; movec   $0809,d0
	move.l  d0,time_stop
	rts

_getstart:
	move.l  time_start,d0
	rts

_getstop:
	move.l  time_stop,d0
	rts
	
_calcpoint:
	fmove   #0,fp4		; znx
	fmove   #0,fp5		; zny
	fmove   fp4,fp6		; zn1x
	fmove   fp5,fp7		; zn1y
	
.loop:
	fmove   fp6,fp2
	fmove   fp7,fp3
	fadd    fp2,fp3		; zn1x+zn1y
	fcmp    #4,fp3	    ; <=4?
	fbgt    .out
	
	fadd    fp5,fp5		;     2*znx
	fmul    fp4,fp5		;     2*znx*zny
	fadd    fp1,fp5		; zny=2*znx*zny+cy
	
	fmove   fp6,fp4
	fsub    fp7,fp4		;     zn1x-zn1y
	fadd	fp0,fp4		; znx=zn1x-zn1y+cx
	
	fmove   fp4,fp6
	fmove   fp5,fp7
	fmul	fp6,fp6		; zn1x=zn1x*zn1x
	fmul    fp7,fp7		; zn1y=zn1y*zn1y
	dbra    d0,.loop
	move.l  #0,d0		; maxit => return 0 (black)
.out:
	rts
	
*******************************

dmacon:		dc.w $8000
intena:		dc.w $8000
oldmode:	dc.w $0000
oldptr: 	dc.l 0
iteration: 	dc.l 0 
aligned:	dc.l 0
modeword    dc.w 0

tempx		dc.w 0
tempy		dc.w 0

time_start	dc.l 0
time_stop	dc.l 0

	end

