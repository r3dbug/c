
#ifndef FLACAT_ASM_H
#define FLACAT_ASM_H

#include "flacat.h"

void __asm add6464(register __a0 ULONG*, register __a1 ULONG*);
void __asm mul646432(register __a0 ULONG*, register __a1 ULONG*, register __d0 ULONG);
void __asm lsr64(register __a0 ULONG*, register __d1 ULONG);
void __asm lsl64(register __a0 ULONG*, register __d1 ULONG);
ULONG __asm LE2BE32(register __d0 ULONG);
UWORD __asm LE2BE16(register __d0 UWORD);
//void __asm LE2BE32BUFFER(register __a0 WORD*, register __d0 ULONG);
UBYTE __asm playstereo(
	register __a0 WORD*,	/* data ptr */ 
	register __d0 ULONG,    /* length */ 
	register __d1 UWORD,    /* volume */
	register __d2 UBYTE,	/* channel */
	register __d5 WORD,		/* mode */
	register __d6 WORD		/* period (frequency) */
	);
void __asm stopplaying(register __d0 UBYTE);
UBYTE __asm mousedown(void);
//unsigned int __asm get_bits_asm(register __a0 void*, register __d0 int, register __d1 int);

unsigned int __asm get_bits_asm(
	register __a0 unsigned char*, // stream_buf
	register __a1 int*, // bit_pos
	register __d0 int, // bits
	register __d1 int); // sign

void __asm SetVampireTaskSwitching(void);

//int __asm get_rice_asm(register __a0 FLAC_DECODER);

int __asm count_zeros(register __a0 FLAC_DECODER);
int __asm calc_param(
	register __d1 unsigned int, // r
	register __d2 int,			// param
	register __d3 unsigned int	// data = getuint(param)
	);

#endif