
#ifndef SPARTAKUS_H
#define SPARTAKUS_H

void QuickSeekCheck(void);
ULONG GetActualPlayPosition(void);
UBYTE GetAndHandleNotIntEvents(UBYTE);
UBYTE isIntEvent(UBYTE);
//MPEGA_STREAM* (*MPEGA_open_ptr)( char *, MPEGA_CTRL *);
UWORD CalcAUDPER(UWORD);
char *FullFilename(char *);
UBYTE __asm playstereo(
	register __a0 WORD*,	/* data ptr */ 
	register __d0 ULONG,    /* length */ 
	register __d1 UWORD,    /* volume */
	register __d2 UBYTE,	/* channel */
	register __d5 WORD,		/* mode */
	register __d6 WORD		/* period (frequency) */
	);
void __asm stopplaying(register __d0 UBYTE);

#endif /* SPARTAKUS_H */