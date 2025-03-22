
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "flacat.h"
#include "flacat_asm.h"

int main(int argc, char *argv[])
{
	FILE *fp;
	int fsize;
	unsigned char *fbuf;
	FLAC_DECODER flacd;
	FILE *wave_fp;
	int wave_size;
	char hdr[64];
	int i, bytes, j;
	int loopcount=0;
	
	fp = fopen(argv[1], "rb");
	if(fp==NULL)
		return -1;
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	fbuf = (unsigned char*)malloc(fsize);
	fread(fbuf, 1, fsize, fp);
	fclose(fp);

	//FLAC_DECODER flacd;
	memset(&flacd, 0, sizeof(flacd));

	if(flac_parse(&flacd, fbuf, fsize)<0){
		printf("Not a FLAC file!\n");
		return -1;
	};

	printf("max_block_size: %d\n", flacd.max_block_size);
	printf("sample_rate: %d\n", flacd.sample_rate);
	printf("channels   : %d\n", flacd.channels);
	printf("sample_bits: %d\n", flacd.sample_bits);
	printf("stream_size: %u\n", flacd.stream_size);

	flacd.out_buf = malloc(flacd.max_block_size * flacd.channels * sizeof(int));

	wave_fp = fopen(argv[2], "wb+");
	wave_size = 0;

	// write wave header
	memset(hdr, 0, 64);

	strcpy(hdr+0x00, "RIFF");
	strcpy(hdr+0x08, "WAVE");
	strcpy(hdr+0x0c, "fmt ");
	*(int*)(hdr+0x10) = LE2BE32(0x10);
	*(short*)(hdr+0x14) = LE2BE16(0x0001);
	*(short*)(hdr+0x16) = LE2BE16(flacd.channels);
	*(int*)(hdr+0x18) = LE2BE32(flacd.sample_rate);
	*(int*)(hdr+0x1c) = LE2BE32(flacd.sample_rate * flacd.channels * (flacd.sample_bits/8));
	*(short*)(hdr+0x20) = LE2BE16(flacd.channels * (flacd.sample_bits/8));
	*(short*)(hdr+0x22) = LE2BE16(flacd.sample_bits);
	strcpy(hdr+0x24, "data");

	fwrite(hdr, 0x2c, 1, wave_fp);

	printf("Decoding frames ...\n");
	while(1)
	{
		if(decode_frame(&flacd)==0)
			break;

		bytes = flacd.sample_bits/8;
		
		for(i=0; i<flacd.block_size; i++)
		{
			int *obuf = flacd.out_buf+i;
			for(j=0; j<flacd.channels; j++)
			{
				*obuf = LE2BE32(*obuf);
				fwrite(obuf, bytes, 1, wave_fp);
				obuf += flacd.block_size;
			}
		}
		
		wave_size += flacd.block_size * flacd.channels * bytes;
		loopcount++;
	}

	fseek(wave_fp, 0, SEEK_SET);
	*(int*)(hdr+0x04) = LE2BE32(wave_size+0x2c-8);
	*(int*)(hdr+0x28) = LE2BE32(wave_size);
	fwrite(hdr, 0x2c, 1, wave_fp);

	fclose(wave_fp);

	return 0;
}

