
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

ULONG FLAC_PCM_BUFFER_SIZE = 25000000;
ULONG FLAC_DATA_PRELOAD = 2000000;
double FLAC_PREDECODE_SECS = 4.0;
ULONG FLAC_DATA_CHUNK_LOAD_SIZE = 18000;		// 17280 bytes needed for 48 khz (0.09 secs / subframe)

/* ARNE modes */
#define ARNE_STEREO				1<<2
#define ARNE_MONO				0<<2
#define ARNE_ONE_SHOT			1<<1
#define ARNE_LOOP				0<<1
#define ARNE_8BIT				1<<0
#define ARNE_16BIT				0<<0

extern double cumulative_duration;
extern ULONG fc[];

double start_duration;

UBYTE PlayFLAC(char* filename)
{
	FILE *fp;
	int fsize;
	unsigned char *fbuf;
	FLAC_DECODER flacd;
	int wave_size;
//	char hdr[64];
	int i, bytes, j;
	int loopcount=0;
	UWORD *pcm_buffer, *pcm_limit, *act_pcm;
//	UBYTE ch=7;
	UBYTE e;
	UBYTE done=0, decode_done=0, loading_done=0;
	ULONG app; // actual play position
	double app_ms;
	double decoding_ratio, decoding_ms, ms_highest=0, ms_lowest=10000000, dec_highest=0, dec_lowest=100000000;
	
	ULONG temp;
	UBYTE sound_on=0;
	int *obuf;
	int cum_fsize, rest_fsize;
	ULONG seek_bitpos_start;
	struct EClockVal crono_start, crono_end;
	double ocd;
	ULONG loading_bps; // bytes per second
	UBYTE switch_volume=FALSE;
	
	//printf("sizeof(int): %u\n", sizeof(int)); // 4 bytes
	SetVampireTaskSwitching();
	
	fp = fopen(filename, "rb");
	if(fp==NULL)
	{
		printf("ERROR opening file %s\n", filename);
		return 0xff;
	}
	
	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fsize < FLAC_DATA_PRELOAD) FLAC_DATA_PRELOAD = fsize;
	
	fbuf = (unsigned char*)malloc(fsize);
	E_Freq=ReadEClock(&crono_start);
	//fread(fbuf, 1, fsize, fp);
	fread(fbuf, 1, FLAC_DATA_PRELOAD, fp);
	cum_fsize=FLAC_DATA_PRELOAD;
	rest_fsize=fsize-cum_fsize;
	E_Freq=ReadEClock(&crono_end);
	temp=EClockDiffInMS(&crono_start, &crono_end, E_Freq);
	loading_bps = (ULONG)((double)FLAC_DATA_PRELOAD / (double)temp * 1000.0);
	if (opt_verbose>=9) printf("PRELOADING: %u bytes in %f secs (%u / sec)\n",
		FLAC_DATA_PRELOAD, (double)temp / 1000.0, loading_bps);
/*	if (loading_bps < 2000000) 
	{
		printf("Device speed: %u bytes / sec => Prebuffer 16 secs ...\n", loading_bps);
		//FLAC_PREDECODE_SECS=16; // slow sd cards ...	
	} //else FLAC_DATA_CHUNK_LOAD_SIZE = 100000;
*/	
	//if (loading_bps < 100000) FLAC_DATA_CHUNK_LOAD_SIZE = 14000; // the strict minimum
	
	memset(&flacd, 0, sizeof(flacd));

	if(flac_parse(&flacd, fbuf, fsize)<0){
		printf("Not a FLAC file!\n");
		return 0xff;
	};

	seek_bitpos_start=flacd.bit_pos;
	
	if (opt_verbose>=9) 
	{
		printf("max_block_size: %d\n", flacd.max_block_size);
		printf("sample_rate: %d\n", flacd.sample_rate);
		printf("channels   : %d\n", flacd.channels);
		printf("sample_bits: %d\n", flacd.sample_bits);
		printf("stream_size: %u\n", flacd.stream_size);
		//printf("duration: %u\n", flacd.pcm_stream_size / flacd.sample_rate);
	}
	
	/* abort if certain criteria are not fulfilled */
	/* (flacat only decodes a subset of all possible flac formats) */
	if (
		((flacd.sample_rate<44100) || (flacd.sample_rate>48000))
	   	||
		(flacd.channels != 2)
		||
		(flacd.sample_bits != 16)
	   )
	{
		printf("FLAC file format not supported (%u Hz / %u channels / %u bits).\n",
			flacd.sample_rate, flacd.channels, flacd.sample_bits);
		goto outahere;
	}
	
	ms_song_duration = (ULONG)((double)flacd.pcm_stream_size / (double)flacd.sample_rate * 1000.0);
	FLAC_PCM_BUFFER_SIZE = flacd.pcm_stream_size * flacd.channels * sizeof(WORD);
	//printf("FLAC_PCM_BUFFER_SIZE: %u\n", FLAC_PCM_BUFFER_SIZE);
	
	flacd.out_buf = malloc(flacd.max_block_size * flacd.channels * sizeof(int));

	wave_size = 0;

	// write wave header
/*	memset(hdr, 0, 64);

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
*/
	
	// allocate pcm buffer
	if (opt_verbose>=9) printf("Predecode buffer (size): needed: %3.2f secs x %d x 4 = %u (fix: %u)\n",
		FLAC_PREDECODE_SECS, flacd.sample_rate, (ULONG)(FLAC_PREDECODE_SECS * (double)flacd.sample_rate * 4.0),
		FLAC_DATA_PRELOAD);
		
	if (!(pcm_buffer=AllocVec(FLAC_PCM_BUFFER_SIZE, MEMF_FAST))) 
	{
		printf("Memory allocation failed.\n");
		return 0xff;
	} //else printf("pcm_buffer allocated (size: %u)\n", FLAC_PCM_BUFFER_SIZE);
	
	act_pcm	= pcm_buffer;
	pcm_limit = pcm_buffer + FLAC_PCM_BUFFER_SIZE;
	
	//printf("Decoding frames ...\n");
	cl_ab=cl_bs=0;
	loopcount=0;
	loading_done = decode_done = done = 0;
	cumulative_duration = 0;
	//act_seek_ms=0;
	
	//printf("flacd.bit_pos: 0x%x (%u)\n", flacd.bit_pos, flacd.bit_pos);

	//CreateSeekTable(&flacd);
	
	// predecode some seconds
/*	while ((cumulative_duration<FLAC_PREDECODE_SECS) && (!decode_done))
	{
		if (decode_frame(&flacd)==0) decode_done=1;	
		for(i=0; i<flacd.block_size; i++)
		{
			int *obuf = flacd.out_buf+i;
			for(j=0; j<flacd.channels; j++)
			{
				*act_pcm=(WORD)*obuf;
				obuf += flacd.block_size;
				act_pcm++;
				if (act_pcm > pcm_limit) act_pcm = pcm_buffer;  // ring buffer
			}
		}
		wave_size += flacd.block_size * flacd.channels * bytes;
	}
	printf("predecode: cumulative_duration: %3.2f memory: %u\n", 
			cumulative_duration, act_pcm-pcm_buffer);
			
	// start playing
	opt_sample_rate = flacd.sample_rate;
	PlayArneRAW(pcm_buffer, FLAC_PCM_BUFFER_SIZE, 0xffff);
	
	// Read rest of the buffer
	temp=fread((UBYTE*)fbuf+FLAC_DATA_PRELOAD, 1, fsize-FLAC_DATA_PRELOAD, fp);
	fclose(fp);
	
	if (temp != fsize-FLAC_DATA_PRELOAD) 
	{
		printf("Error loading data: size: %u (expected: %u)\n", 
			temp, fsize-FLAC_DATA_PRELOAD);	
		goto outahere;
	}	
*/	
reseek:	
	
	if (opt_seek_ms != 0)
	{
		if (opt_seek_ms>ms_song_duration) opt_seek_ms = ms_song_duration;
		temp=(ULONG)(opt_seek_ms / 1000.0);
		flacd.bit_pos=flacd.st[temp].bit_pos;
		start_duration=cumulative_duration=flacd.st[temp].secs;
	
		act_seek_ms=start_duration * 1000;
	}
	else act_seek_ms=start_duration=0;
	
	cl_bs=clock();
    E_Freq=ReadEClock(&ecl_bs);
					
	do
	{
	
		if (((app=GetActualPlayPosition())) >= ms_song_duration-1000.0) done=1;
        else 
        {
        
			
			//printf("ms_song_duration: %u app: %u cumulative_duration: %3.2f\n", 
			//	ms_song_duration, app, cumulative_duration);
				
			//for (i=0; i<10; i++)
			//{
			
			if (/*(sound_on) &&*/ (rest_fsize))
			{
				temp=fread((UBYTE*)fbuf+cum_fsize, 1, FLAC_DATA_CHUNK_LOAD_SIZE, fp);
				cum_fsize += FLAC_DATA_CHUNK_LOAD_SIZE;
				rest_fsize = fsize - cum_fsize;
				
				if (temp==0) 
				{
					rest_fsize=0;
					cum_fsize=fsize;
					//CreateSeekTable(&flacd);
					//printf("Seek table created\n");
					fclose(fp);
					if (opt_verbose>=8) printf("\n*\n*\n*\nFLAC data completely loaded.\n*\n");
					loading_done = 1;
					//printf("app: %u\n", app);
				}
			}
			
			ocd=cumulative_duration;
			if ((!decode_done) && (decode_frame(&flacd)==0))
			{
					if (opt_verbose>=8) printf("\n*\n*\n*\nDecode done!!!\n*\n");
					decode_done = 1;
			}
			else
			{
				if (!decode_done)
				{
					for(i=0; i<flacd.block_size; i++)
					{
						/*int **/obuf = flacd.out_buf+i;
						for(j=0; j<flacd.channels; j++)
						{
							*act_pcm=(WORD)*obuf;
							obuf += flacd.block_size;
							act_pcm++;
							if (act_pcm > pcm_limit) act_pcm = pcm_buffer;  // ring buffer
						}
					}
				}
				temp=GetActualPlayPosition();
				
				decoding_ms = (double)temp-(double)app;
				decoding_ratio = (decoding_ms/1000.0) / (cumulative_duration-ocd);
				
				if (decoding_ms > ms_highest) ms_highest = decoding_ms;
				if ((decoding_ms < ms_lowest) && (decoding_ms>20)) ms_lowest = decoding_ms;
				if (decoding_ratio > dec_highest) dec_highest = decoding_ratio;
				if ((decoding_ratio < dec_lowest) && (decoding_ms > 20)) dec_lowest = decoding_ratio;
		
				if ((opt_verbose>=9)/* || (loading_done)*/) printf("ocd: %3.2f ncd: %3.2f => diff: %3.2f\n", 
					ocd, cumulative_duration, cumulative_duration - ocd);
				
				if ((opt_verbose>=9)/* ||(loading_done)*/) 
				{
					
					
					printf("Loading+decoding: %u ms => ratio: %f (faster < 1.0 < slower than playing)\n",
						temp-app, ((double)(temp-app)/1000.0) / (cumulative_duration-ocd));
				}
			}
			
			bytes = flacd.sample_bits/8;
		
			wave_size += flacd.block_size * flacd.channels * bytes;
			loopcount++;
			if (((!sound_on) || (switch_volume)) && (cumulative_duration-start_duration>FLAC_PREDECODE_SECS))
			{
				opt_sample_rate = flacd.sample_rate;
				if (switch_volume) 
				{
					stopplaying(opt_ARNE_main);
					sound_on=0;
					switch_volume=0;	
				}
				PlayArneRAW(pcm_buffer, FLAC_PCM_BUFFER_SIZE >> 2, opt_volume);
				sound_on=1;
				//ch=playstereo(pcm_buffer, FLAC_PCM_BUFFER_SIZE, 0xffff, ch, ARNE_STEREO|ARNE_LOOP|ARNE_16BIT, 
				//	CalcAUDPER(flacd.sample_rate));
			}
			
			/*if (
				((loading_done) && ((loopcount % 10) == 0))
			   ||
			   (!loading_done)
			   )
			{*/	
			e=GetAndHandleNotIntEvents(1); 
        	switch (e)
        	{
        	    case EVENT_BREAK : 
        	    case EVENT_NEXT_SONG : 
        	    case EVENT_PREVIOUS_SONG : done=1; opt_seek_ms=0; break;
        	
				case EVENT_EXECUTE_SEEK : EnterNormal();
										  printf("\r->| %3.2f secs\n", (double)ctrl_seek/1000.0);
						
										  stopplaying(opt_ARNE_main);
										  sound_on=0;
										  
										  if (flacd.st==NULL)
										  {
												// seek table doesn't exist
												// => create one
												
												if (!loading_done)
												{
													// ont all FLAC data has been loaded
													// => first load data
													temp=fread((UBYTE*)fbuf+cum_fsize, 1, rest_fsize, fp);
													cum_fsize = fsize;
													rest_fsize = 0;
													loading_done=1;
													fclose(fp);
													if (opt_verbose>=9) printf("\n*\n*\n*\nFLAC data completely loaded.\n*\n");
												}				  
										  
										  		CreateSeekTable(&flacd);
										  }
						
										  if (opt_verbose>=9) printf("ctrl_seek: %u\n", ctrl_seek);
						
										  opt_seek_ms=ctrl_seek;
										  act_pcm	= pcm_buffer;
										  pcm_limit = pcm_buffer + FLAC_PCM_BUFFER_SIZE;
	
										  goto reseek;
                                          break;
					
										  
				case EVENT_EXECUTE_VOLUME : 
											EnterNormal();
											opt_volume=(UWORD)ctrl_volume | (UWORD)(ctrl_volume & 0xff)<<8;
											printf("\r-/+ %u%\n", (ULONG)((100.0 / 255.0) * (double)ctrl_volume));
											
											stopplaying(opt_ARNE_main);
										    sound_on=0;
											
											// we need a seek table to change the volume
											if (flacd.st==NULL)
										  	{
												// seek table doesn't exist
												// => create one
												
												if (!loading_done)
												{
													// ont all FLAC data has been loaded
													// => first load data
													temp=fread((UBYTE*)fbuf+cum_fsize, 1, rest_fsize, fp);
													cum_fsize = fsize;
													rest_fsize = 0;
													loading_done=1;
													fclose(fp);
													if (opt_verbose>=9) printf("\n*\n*\n*\nFLAC data completely loaded.\n*\n");
												}				  
										  
										  		CreateSeekTable(&flacd);
										  	}
											
											act_pcm	= pcm_buffer;
										  	pcm_limit = pcm_buffer + FLAC_PCM_BUFFER_SIZE;
											
											ctrl_seek=GetActualPlayPosition(); // - 50; // + cl_volume_change_delta; //2250;
											//printf("execute volume ... ctrl_seek: %u\n", ctrl_seek);
											opt_seek_ms = ctrl_seek;
											
											goto reseek;
											break;	
			
			}
		}
		
		app_ms=((double)app/1000.0);
		if ((sound_on) && (app_ms > 6.0) && (app_ms>cumulative_duration-0.5))
		{
			printf("Device speed: %u bytes/sec - decoding failed.\n", loading_bps);
			printf("Positions: Playing: %3.2f Decoding: %3.2f (ms: %u/%u ratio: %2.2f/%2.2f)\n",
				app_ms, cumulative_duration, (ULONG)ms_highest, (ULONG)ms_lowest, dec_highest, dec_lowest); 
			
			//stopplaying(opt_ARNE_main);
			//sound_on=0;
			goto outahere;
		}
			
		if (decode_done) Delay(2);
	} while (!done);

outahere:

	if (sound_on) stopplaying(opt_ARNE_main); 
	sound_on=0;
	
	opt_seek_ms=0;
	    
	//*(int*)(hdr+0x04) = LE2BE32(wave_size+0x2c-8);
	//*(int*)(hdr+0x28) = LE2BE32(wave_size);

	FreeSeekTable(&flacd);
	
	if (pcm_buffer) FreeVec(pcm_buffer);
	if (flacd.out_buf) free(flacd.out_buf);
	if (fbuf) free(fbuf);
/*	
	for (i=1; i<=7; i++)
	{
		printf("fc[%d]: %u\n", i, fc[i]);
	}
*/	
	return e;
}

