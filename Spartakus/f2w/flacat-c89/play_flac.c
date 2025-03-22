
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "flacat.h"
#include "flacat_asm.h"
#include <exec/memory.h>
#include <exec/exec.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <pragmas/exec_pragmas.h>

#define PCM_BUFFER_SIZE	25000000

/* ARNE modes */
#define ARNE_STEREO				1<<2
#define ARNE_MONO				0<<2
#define ARNE_ONE_SHOT			1<<1
#define ARNE_LOOP				0<<1
#define ARNE_8BIT				1<<0
#define ARNE_16BIT				0<<0

UWORD CalcAUDPER(UWORD sr)
{
	/* calculate AUDPER for ARNE from sample rate (sr) / frequency */
	/* examples: 
	 * - 44.1 khz: 80
	 * - 48 khz: 74
	 * - 56 khz: 65 (?)
	 */
	
	ULONG i=3552000; //3528000; // 3600000; // what's the correct value here?!
	UWORD v;
	
	switch (sr)
	{
		//case 44100 : v=80; break;
		//case 48000 : v=74; break;
		default : v=i/sr;
	}
	//printf("SR: %u => WORD: %u\n", sr, v);
	return v;
}

int main(int argc, char *argv[])
{
	FILE *fp;
	int fsize;
	unsigned char *fbuf;
	FLAC_DECODER flacd;
	int wave_size;
	char hdr[64];
	int i, bytes, j;
	int loopcount=0;
	UWORD *pcm_buffer, *pcm_limit, *act_pcm;
	UBYTE ch=7;
	
	fp = fopen(argv[1], "rb");
	if(fp==NULL)
		return -1;
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	fbuf = (unsigned char*)malloc(fsize);
	fread(fbuf, 1, fsize, fp);
	fclose(fp);

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

	// allocate pcm buffer
	if (!(pcm_buffer=AllocVec(PCM_BUFFER_SIZE, MEMF_FAST))) 
		return -1;
	act_pcm	= pcm_buffer;
	pcm_limit = pcm_buffer + PCM_BUFFER_SIZE - 8;
	
	printf("Decoding frames ...\n");
	loopcount=0;
	while(1)
	{
		if(decode_frame(&flacd)==0)
			break;

		bytes = flacd.sample_bits/8;
		
		if ((loopcount==0) && (i<2)) printf("BlockSize: %u\n", flacd.block_size);
				
		for(i=0; i<flacd.block_size; i++)
		{
			int *obuf = flacd.out_buf+i;
			for(j=0; j<flacd.channels; j++)
			{
				//*obuf = LE2BE32(*obuf);
				*act_pcm=(WORD)*obuf;
				//if ((loopcount==0) && (i<2)) printf("WORD[%d][%d]: %04x\n", i, j, (WORD)*obuf);
				
				//fwrite(obuf, bytes, 1, wave_fp);
				obuf += flacd.block_size;
				act_pcm++;
				if (act_pcm > pcm_limit) act_pcm = pcm_buffer;  // ring buffer
			}
		}
		
		if (mousedown()) {
			stopplaying(ch);
			break;
		}
		
		wave_size += flacd.block_size * flacd.channels * bytes;
		loopcount++;
		if (loopcount==300) 
		{
			ch=playstereo(pcm_buffer, PCM_BUFFER_SIZE, 0xffff, ch, ARNE_STEREO|ARNE_LOOP|ARNE_16BIT, 
				CalcAUDPER(flacd.sample_rate));
            
		}
		/*else if (loopcount == 1000)
		{
			stopplaying(ch);
			break;
		}
		*/
	}

	printf("loopcount: %u\n", loopcount);
	
	*(int*)(hdr+0x04) = LE2BE32(wave_size+0x2c-8);
	*(int*)(hdr+0x28) = LE2BE32(wave_size);

	FreeVec(pcm_buffer);
	
	return 0;
}

