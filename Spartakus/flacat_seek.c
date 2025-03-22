/*
 * This code was originally taken from:
 * https://github.com/tpunix/flacat
 *
 * It is based on the following two implementations (Java/Python)
 * for a simple and dependency-free flac algorithm: 
 */

/* 
 * Simple FLAC decoder (Java)
 * 
 * Copyright (c) 2017 Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/simple-flac-implementation
 */

/*
 * # Simple FLAC decoder (Python)
 * # 
 * # Copyright (c) 2020 Project Nayuki. (MIT License)
 * # https://www.nayuki.io/page/simple-flac-implementation
 */

/* The C version was then ported to SAS/C (C89) and
 * the Vampire.
 *
 * (c) RedBug aka Marcel Maci (m.maci@gmx.ch)
 *
 * The following copyright notice, published by the original
 * author, also applies to the Vampire version (including all
 * source files it contains).
 */

/* 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "flacat_seek.h"

double song_duration=SEEK_SECONDS; 
UBYTE seek_mode_on=1;
ULONG sps;

UBYTE NumberOfLeadingZeros(ULONG v)
{
	int i=32, found=0;
	while ((!found) && (--i>=0))
	{
		found = (1 << i) & v;
	}
	return 32-(i+1);
}

/******************************************************************************/

static unsigned int __get_bits(FLAC_DECODER *flacd, int bits, int sign)
{
	int r = 0;
	int s = 0;
	int B = flacd->bit_pos>>3;
	int b = 8-(flacd->bit_pos&7);

	flacd->bit_pos += bits;
	
	while(s<bits)
	{
		s += b;
		if(s>32)
		{
			r |= flacd->stream_buf[B++] >> (s-32);
			s = 32;
		}
		else
		{
			r |= flacd->stream_buf[B++] << (32-s);
		}
		b = 8;
	}

	if(sign)
	{
		r = r >> (32-bits);
	}
	else
	{
		r = ((unsigned int)r) >> (32-bits);
	}
	return r;
}


#define get_uint(b)  __get_bits(flacd, b, 0)
#define get_sint(b)  __get_bits(flacd, b, 1)

static int get_rice(FLAC_DECODER *flacd, int param)
{
	unsigned int r = 0;
	while(get_uint(1)==0) r++;
	r = (r<<param) | get_uint(param);
	r = (r>>1) ^ (-(r&1));
	return r;
}


ULONG GetFrameSampleRate(ULONG v)
{
	ULONG sr[]={0,88200,176400,192000,8000,16000,22050,24000,32000,44100,48000,96000,8,16,10,0};
	return sr[v];
	
/*	ULONG r;
	switch (v)
	{
		case 1 : r=88200; break;	
		case 2 : r=176400; break;	
		case 3 : r=192000; break;	
		case 4 : r=8000; break;	
		case 5 : r=16000; break;	
		case 6 : r=22050; break;	
		case 7 : r=24000; break;	
		case 8 : r=32000; break;	
		case 9 : r=44100; break;	
		case 10 : r=48000; break;	
		case 11 : r=96000; break;	
		case 12 : r=8; break;			// uncommon sample rate (stored as an 8 bit number)	
		case 13 : r=16; break;			// uncommon sample rate (stored as a 16 bit number)	
		case 14 : r=10; break;			// idem (divided by 10, stored as a 16 bit number)	
		case 15 : r=0; break;			// forbidden	
		
	}
	return r;
*/
}

UBYTE GetSampleSizeFromBits(UBYTE b)
{
	UBYTE s[] = {0,8,12,0,16,20,24,32};
	return s[b];
}

ULONG GetBlockSizeFromBits(UBYTE* p, UBYTE bits)
{
	ULONG bs;
	if(bits<2) bs = 192;
	else if (bits<6) bs = 576<<(bits-2);
	else if (bits==6) bs = p[5];
	else if (bits==7) bs = *((UWORD*)&p[5]);
	else bs = 256<<(bits-8);
	return bs;
}


ULONG SeekFrames(FLAC_DECODER *flacd)
{
	ULONG old_bitpos=flacd->bit_pos;
	ULONG act_bytepos = flacd->bit_pos>>3;
	UBYTE check_ok;
	ULONG temp;
	UBYTE bits_bs;
	UBYTE *p;
	ULONG bs, sr;
	ULONG css=0; // umulative_stream_size
	UWORD act_secs=sps+1;
	
	song_duration=0; //SEEK_SECONDS;
	
	//printf("SeekFrames: bit_pos: %u (byte position): %u\n", 
	//	flacd->bit_pos, flacd->bit_pos>>3);
	
	// 1st frame
	while (*(UWORD*)(&flacd->stream_buf[act_bytepos]) != 0xfff8) act_bytepos++;
	
	//flacd->st[act_secs].bit_pos=flacd->bit_pos; //flacd->bit_pos;
	//flacd->st[act_secs].secs=song_duration;
	//act_bytepos++;
	act_secs=0;
	
			
	while (1) 
	{
		// write entries (first = position 0 must be valid)
		//printf("act_secs: %u song_duration: %f song_duration: %u\n", 
		//	act_secs, song_duration, (UWORD)song_duration);
		
		/* use some data in the header to confirm it is the beginning of a frame */
		check_ok=TRUE;
		p=&flacd->stream_buf[act_bytepos];
		// sample rate
		temp=p[2] & 0xf;
		sr=GetFrameSampleRate(temp);
		if (sr != flacd->sample_rate) check_ok=FALSE;
		// sample size
		temp=(p[3] & 0xf) >> 1;
		if (GetSampleSizeFromBits(temp) != flacd->sample_bits) check_ok=FALSE;
		//printf("temp: %u %u\n", temp, GetSampleSizeFromBits(temp));
		
		// calculate blocksize
		bits_bs=(p[2] & 0xf0) >> 4;
		bs=GetBlockSizeFromBits(p, bits_bs);
		//printf("bits_bs: %u blocksize: %u\n", bits_bs, bs);
		
		if (check_ok) 
		{
			if (act_secs<5) printf("Sync: act_bytepos: %x\n", act_bytepos);
			
			if (act_secs==(UWORD)song_duration)
			{
				flacd->st[act_secs].bit_pos=act_bytepos<<3; //flacd->bit_pos;
				flacd->st[act_secs].secs=song_duration;
				if (act_secs < 5) printf("flacd->st[%d].bit_pos: %x (byte pos: %x) .secs: %f\n", 
					act_secs,
					flacd->st[act_secs].bit_pos,
					flacd->st[act_secs].bit_pos>>3,
					flacd->st[act_secs].secs);
				
				act_secs++;	// next entry
			}
			css += bs;
			song_duration += (double)bs / (double)sr; 
			//printf("song_duration (cumulated): %f\n", song_duration);
		}
		
		act_bytepos++;
		if (css>flacd->pcm_stream_size) break;
		
		while (*(UWORD*)(&flacd->stream_buf[act_bytepos]) != 0xfff8) act_bytepos++;
		//if (song_duration>30) break;
	}
	printf("song_duration (SEEK_TABLE): %f\n", song_duration);
	song_duration=SEEK_SECONDS;
}

SEEK_TABLE* CreateSeekTable(FLAC_DECODER *flacd)
{
	// bit_pos must point to the 1st frame
	ULONG secs;
	SEEK_TABLE *st;
	
	secs = flacd->pcm_stream_size / flacd->sample_bits + 1;
	printf("size: %u / bits: %u\n", flacd->pcm_stream_size, flacd->sample_bits);
	
	printf("SEEK_TABLE: %u secs\n", secs);
	
	if (secs < 10) return NULL;
	
	
	if (!(flacd->st=(SEEK_TABLE*)malloc/*AllocVec*/(sizeof(SEEK_TABLE)*secs))) //, MEMF_FAST)))
		return NULL;
	
	SeekFrames(flacd);
	
	return flacd->st;
}

void FreeSeekTable(FLAC_DECODER *flacd)
{
	free/*FreeVec*/(flacd->st);
}

/******************************************************************************/


static void decode_residuals(FLAC_DECODER *flacd, int order, int *obuf)
{
	int method = get_uint(2);
	int param_bits = (method)? 5: 4;
	int escape = (method)? 0x1f: 0x0f;

	int parts = 1<<get_uint(4);
	int psize = flacd->block_size/parts;

	int i, j;
	int start, end, param, nbits;
	
	for(i=0; i<parts; i++)
	{
		/*int*/ start = i*psize+((i==0)?order:0);
		/*int*/ end = (i+1)*psize;
		/*int*/ param = get_uint(param_bits);
		if(param<escape)
		{
			for(j=start; j<end; j++)
			{
				obuf[j] = get_rice(flacd, param);
			}
		}
		else
		{
			/*int*/ nbits = get_uint(5);
			if (seek_mode_on) flacd->bit_pos += (nbits*(end-start));
			else {
			for(j=start; j<end; j++)
			{
				obuf[j] = get_sint(nbits);
			} }
		}
	}
}


static void restore_lpc(FLAC_DECODER *flacd, int *coefs, int order, int shift, int *obuf)
{
	int i, j;
	LONG64 sum, mul, lobuf;
	
	for(i=order; i<flacd->block_size; i++){
		sum[0] = 0; sum[1] = 0;
		for(j=0; j<order; j++)
		{
			lobuf[0]=0; lobuf[1]=obuf[i-1-j];
			mul646432(mul,lobuf,coefs[j]);
			add6464(sum,mul);
		}
		lsr64(sum,shift);
		obuf[i] += sum[1];
	}
}

/******************************************************************************/


static int fixed_coef[5][4] = {
	{0,},
	{1,},
	{2, -1},
	{3, -3, 1},
	{4, -6, 4, -1},
};


static void decode_lpc(FLAC_DECODER *flacd, int sample_bits, int order, int fixed, int *obuf)
{
	int i, precision, shift;
	int var_coefs[32];
	int *coefs;

	if (seek_mode_on) flacd->bit_pos += order*sample_bits;
	else {
	for(i=0; i<order; i++)
	{
		obuf[i] = get_sint(sample_bits);
	} }

	if(fixed)
	{
		coefs = fixed_coef[order];
		shift = 0;
	}
	else
	{
		precision = get_uint(4)+1;
		shift = get_sint(5);
		if (seek_mode_on) flacd->bit_pos += order*precision;
		else {
		for(i=0; i<order; i++)
		{
			var_coefs[i] = get_sint(precision);
		} }
		coefs = var_coefs;
	}

	decode_residuals(flacd, order, obuf);
	if (!seek_mode_on) restore_lpc(flacd, coefs, order, shift, obuf);
}


/******************************************************************************/


int decode_subframe(FLAC_DECODER *flacd, int sample_bits, int *obuf)
{
	int i, type, shift;
	
	get_uint(1);

	type = get_uint(6);
	shift = get_uint(1);
	if(shift) 
	{
		while(get_uint(1)==0)
		{
			shift++;
		}
	}
	sample_bits -= shift;

	if(type==0)
	{
		// Constant coding
		int const_data = get_sint(sample_bits);
		if (!seek_mode_on) {
		for(i=0; i<flacd->block_size; i++)
		{
			obuf[i] = const_data;
		} }
	}
	else if(type==1)
	{
		// Verbatim coding
		if (seek_mode_on) flacd->bit_pos += flacd->block_size*sample_bits;
		else {
		for(i=0; i<flacd->block_size; i++)
		{
			obuf[i] = get_sint(sample_bits);
		} }
	}
	else if(type>=8 && type<=12)
	{
		// Fixed LPC coding
		decode_lpc(flacd, sample_bits, type-8, 1, obuf);
	}
	else
	{
		decode_lpc(flacd, sample_bits, type-31, 0, obuf);
	}

	for(i=0; i<flacd->block_size; i++)
	{
		obuf[i] <<= shift;
	}

	return 0;
}


static char depth_table[8] = {
	0, 8, 12, 0, 16, 20, 24, 32
};

int decode_frame(FLAC_DECODER *flacd)
{
	int i, sample_bits, sync, bsize_code, chmode, samp_code, seq;
	int frame_sample_rate;
	int *out_buf0;
	int *out_buf1;
	int n;
	int side, right;
	ULONG old_bitpos;
	static ULONG act_secs = 0;
	static ULONG errors = 0;
		
	//printf("Frame @ %08x (bit_pos: %x)\n", flacd->bit_pos>>3, flacd->bit_pos);
	// check seek table
	//printf("act_secs: %u st->secs %f\n", act_secs, flacd->st[act_secs].secs);
	
	//printf("act_secs: %u song_duration: %3.2f\n", act_secs, song_duration);
	
	/*
	if (act_secs == (ULONG)song_duration)
	{
		printf("CHECK ... \n");
		
		if (
			(flacd->bit_pos != flacd->st[act_secs].bit_pos)
			||
			(song_duration != flacd->st[act_secs].secs)
		   )
		{
			
			printf("SEEK ERROR\n");
			printf("[%03u]: flacd->bit_pos: %u vs flacd->st[%u].bit_pos: %u\n",
				act_secs, flacd->bit_pos, act_secs, flacd->st[act_secs].bit_pos);
			printf("[%03u]: song_duration: %3.5f vs flacd->st[%u].secs: %3.5f\n",
				act_secs, song_duration, act_secs, flacd->st[act_secs].secs);
			
		} else printf("ALL GOOD! (act_secs: %u, song_duration: %3.5f (%3.5f) errors=%u)\n", 
			act_secs, song_duration, flacd->st[act_secs].secs, errors);
		act_secs++;
	}
	*/
	
	old_bitpos = flacd->bit_pos;
	
	if(flacd->bit_pos >= flacd->stream_size)
		return 0;

	sync = get_uint(16);
	if(sync!=0xfff8)
	{
		return 0;
	}

	bsize_code = get_uint(4);
	//printf("bsize_code: %u\n", bsize_code);
	
	frame_sample_rate = GetFrameSampleRate(get_uint(4));
	chmode     = get_uint(4);
	samp_code  = get_uint(3);
	get_uint(1);
	//printf("frame_sample_rate: %u\n", frame_sample_rate);
	
	seq = get_uint(8);
	if(seq>=0x80)
	{
		/*int*/ n = NumberOfLeadingZeros(~(seq<<24)) - 1;
		//int n = __builtin_clz(~(seq<<24)) -1;			// clz = count leading zeros
		flacd->bit_pos += n*8;
	}

	if(bsize_code<2)
	{
		flacd->block_size = 192;
	}
	else if(bsize_code<6)
	{
		flacd->block_size = 576<<(bsize_code-2);
	}
	else if(bsize_code==6)
	{
		flacd->block_size = get_uint(8)+1;
	}
	else if(bsize_code==7)
	{
		flacd->block_size = get_uint(16)+1;
	}
	else
	{
		flacd->block_size = 256<<(bsize_code-8);
	}

//	printf("flacd->block_size (frame): %u\n", flacd->block_size);
	// number of (fraction of) seconds the frame adds to song duration
	song_duration += (double)flacd->block_size / (double)frame_sample_rate; 
//	printf("song_duration (cumulated): %f\n", song_duration);
	
	sample_bits = flacd->sample_bits;
	if(depth_table[samp_code])
	{
		sample_bits = depth_table[samp_code];
	}

	get_uint(8); // CRC-8

	if(chmode<8)
	{
		for(i=0; i<(chmode+1); i++)
		{
			decode_subframe(flacd, sample_bits, flacd->out_buf + i*flacd->block_size);
		}
	}
	else
	{
		/*int **/out_buf0 = flacd->out_buf;
		/*int **/out_buf1 = out_buf0 + flacd->block_size;
		decode_subframe(flacd, sample_bits+((chmode==9)?1:0), out_buf0);
		decode_subframe(flacd, sample_bits+((chmode==9)?0:1), out_buf1);

		if (!seek_mode_on) {
		if(chmode==8)
		{
			for(i=0; i<flacd->block_size; i++)
			{
				out_buf1[i] = out_buf0[i] - out_buf1[i];
			}
		}
		else if(chmode==9)
		{
			for(i=0; i<flacd->block_size; i++)
			{
				out_buf0[i] += out_buf1[i];
			}
		}
		else
		{
			for(i=0; i<flacd->block_size; i++)
			{
				/*int*/ side  = out_buf1[i];
				/*int*/ right = out_buf0[i] - (side>>1);
				out_buf1[i] = right;
				out_buf0[i] = right + side;
			}
		} }
	}

	flacd->bit_pos = (flacd->bit_pos+7)&~7;
	get_uint(16); // crc16

	//printf("Frame @ %08x (bit_pos: %x) Delta: %u\n", flacd->bit_pos>>3, flacd->bit_pos, flacd->bit_pos-old_bitpos);

	return 1;
}


/******************************************************************************/

int flac_parse(FLAC_DECODER *flacd, unsigned char *ibuf, int isize)
{
	int finish;
	
	flacd->stream_buf = ibuf;
	flacd->stream_size = isize*8;		// wrong!
	flacd->bit_pos = 0;

	if(get_uint(32)!=0x664c6143)
		return -1;

	finish = 0;
	while(finish==0)
	{
		int type = get_uint(8);
		int size = get_uint(24);
		int next_pos = flacd->bit_pos+size*8;

		finish = type&0x80;
		type &= 0x7f;
		printf("META type:%02x\n", type);

		// STREAMINFO
		if(type==0)
		{
			get_uint(16);
			flacd->max_block_size = get_uint(16);
			get_uint(24);
			get_uint(24);
			flacd->sample_rate = get_uint(20);
			flacd->channels    = get_uint(3) + 1;
			flacd->sample_bits = get_uint(5) + 1;
			get_uint(4);
			flacd->pcm_stream_size = get_uint(32);


		}

		flacd->bit_pos = next_pos;
	}

	return 0;
}

/******************************************************************************/

