
#ifndef _FLACAT_H_
#define _FLACAT_H_

#include <exec/types.h>
#include "flacat_asm.h"

#define SEEK_SECONDS 		0 //30.615510
#define SEEK_BITPOSITION	0 //0x16b7858

typedef ULONG LONG64[2];

typedef struct {
	ULONG bit_pos;
	double secs;
} SEEK_TABLE;

typedef struct _FLAC_DECODER {
	unsigned char *stream_buf;
	int bit_pos;
	int stream_size;
	int pcm_stream_size;

	int sample_rate;
	int channels;
	int sample_bits;

	int max_block_size;
	int block_size;

	int *out_buf;
	void (*frame_out)(struct _FLAC_DECODER *);
	
	int st_bit_pos;			// starting bitpos for seek table
	SEEK_TABLE *st;
} FLAC_DECODER;

int flac_parse(FLAC_DECODER *flacd, unsigned char *ibuf, int isize);
int decode_frame(FLAC_DECODER *flacd);

ULONG SeekFrames(FLAC_DECODER *flacd);
SEEK_TABLE* CreateSeekTable(FLAC_DECODER *);
void FreeSeekTable(FLAC_DECODER *);

#endif

