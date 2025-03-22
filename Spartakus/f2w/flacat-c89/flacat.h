
#ifndef _FLACAT_H_
#define _FLACAT_H_

#include <exec/types.h>
#include "flacat_asm.h"

typedef ULONG LONG64[2];

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
} FLAC_DECODER;

int flac_parse(FLAC_DECODER *flacd, unsigned char *ibuf, int isize);
int decode_frame(FLAC_DECODER *flacd);

#endif

