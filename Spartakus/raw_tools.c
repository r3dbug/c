
#include "raw_tools.h"
#include "events.h"

#define DATA_TAG 0x64617461 /* WAV */
#define COMM_TAG 0x434f4d4d /* AIFF */
#define SSND_TAG 0x53534e44 /* AIFF */
/*
ULONG LE2BE_LC(ULONG le)
{
    ULONG be=0;
    
    be = ((le & 0xff) << 24) | 
         ((le & 0xff00) << 8) | 
         ((le & 0xff0000) >> 8) | 
         ((le & 0xff000000) >> 24);
         
    return be; 
}

UWORD LE2BE_WC(UWORD le)
{
    UWORD be = 0;
    
    be = ((le & 0xff) << 8) | ((le & 0xff00) >> 8);
    
    return be;
}
*/
void* SearchDataTag(ULONG* d)
{
    ULONG i=0;
    UWORD* dw=(void*)d;
    ULONG* dl=d;
    
    //printf("test: %08x\n", dl[i]);
    
    while ((*dl != DATA_TAG) && (i < 140))
    {
        i++;
        dl = (ULONG*)(dw+i);
        //printf("test: %08x\n", *dl);
    }
    return dl;
}

void* SearchChunk(UWORD* buffer, ULONG ckID)
{
	ULONG p=0;
	while ((*((ULONG*)(buffer+p))!=ckID) && (p<100)) { /*printf("%08x\n", *((ULONG*)(buffer+p)));*/ p++; }
	if (p<100) return buffer+p;
	else return NULL;
}

ULONG MS2SWO(ULONG ms)  /* micro seconds => stereo word offset ("pcm pairs") */
{
	return (ULONG)((double)ms * 44.1 * 2.0);
}

ULONG MS2SBO(ULONG ms)  /* same, but as byte offset */
{
	return (ULONG)((double)ms * 44.1 * 4.0);
}

/*
void PlayArneRAW(UWORD* pcm, ULONG size, UWORD volume)
{
    UBYTE i,j=0;
    do 
    {
        i=playstereo((WORD*)pcm, size, volume, opt_ARNE_main, ARNE_STEREO | ARNE_16BIT | ARNE_ONE_SHOT, CalcAUDPER(opt_sample_rate));
        cl_bs=clock();
        E_Freq=ReadEClock(&ecl_bs);
		
        if (i==0xff)
        {
            // channel not free 
            if (!j)
            {
                // first timy it happens => start search at channel 4 
                opt_ARNE_main=4;
                j=1;
            }
            else
            {
                // if channel 4 is not free neither try +1 up to available 15 channels    
                opt_ARNE_main++;    
            }
        }
    } while ((i==0xff) && (opt_ARNE_main<=15));
        
    opt_ARNE_main=i;
}
*/

UWORD* PrepareNewSeekDestinationWAV(UWORD* pcm_start, LONG ms_position)
{
    ULONG seek_offset;
    
	if (opt_verbose>=9) printf("\nNew seek position\n");
	
    if ((ms_position < ms_song_duration) && (ms_position >= 0))
    {
        act_seek_ms=ms_position;
        cumulative_seek_ms=0;
        seek_offset = MS2SWO(ms_position); //(double)ms_position * 44.1 * 2; // in pcm words
        
        return pcm_start + seek_offset ;
		
    } else return NULL;
}


   
UBYTE PlayRAW(char* filename, UBYTE type)
{
    FILE *fh;
    ULONG size;
    UBYTE *buffer;
    ULONG counter, t;
    struct WAVHeader* wh;
    UBYTE e=0;
    ULONG* data_tag;
    ULONG  data_size;
    UWORD* pcm_data_start; 
    UWORD* pcm_seek_start;
    ULONG first_block=4000000; //5644800; // 32 seconds of prebuffering
	ULONG act_block, block_size=4000000; // sd cards are too slow to read rest of file in one block
    UBYTE done=0;
	struct SoundDataChunk* sdc;
	struct CommonChunk* commck;
	
    fh=fopen(filename,"r");
    if (opt_verbose>=9) printf("Open: %s\n", filename);
    
    fseek(fh,0,2);
    size=ftell(fh);
    rewind(fh);
    
    buffer=(UBYTE*)AllocVec(size + block_size, MEMF_ANY | MEMF_CLEAR); // block_size as safety buffer ...
    if (opt_verbose>=9) printf("Buffer: %p\n");
    
    if (size > 5000000) first_block=size/10;
    else first_block=size;
    if (opt_verbose>=9) printf("First block (10%): %u\n", first_block);
    t=fread(buffer,1,first_block,fh);
    if (opt_verbose>=9) printf("size: %u (first_block: %u t: %u)\n", size, first_block, t);
    
    switch (type)
	{
		case STREAM_TYPE_WAV :
		
			wh=(struct WAVHeader*)buffer;
    
    		if (opt_verbose>=9) printf("Header:\n");
    		if (opt_verbose>=9) printf("FileTypeBlocID: %x\n", wh->FileTypeBlocID);
    		if (opt_verbose>=9) printf("FileSize: %u\n", LE2BE_LA(wh->FileSize));
    		if (opt_verbose>=9) printf("AudioFormat: %u (1=pcm/2=Float)\n", LE2BE_WA(wh->AudioFormat));
    		if (opt_verbose>=9) printf("NbrChannels: %u (1=mono/2=stereo)\n", LE2BE_WA(wh->NbrChannels));
    		if (opt_verbose>=9) printf("Frequency: %u\n", LE2BE_LA(wh->Frequency));
    		if (opt_verbose>=9) printf("DataBlocID: %x\n", wh->DataBlocID);
    		
			opt_sample_rate = LE2BE_LA(wh->Frequency);
			
			/*
    		if (LE2BE_LA(wh->Frequency) != 44100)
    		{
    		    printf("Frequency (%u) is not supported.\n", LE2BE_LA(wh->Frequency));
    		    goto outahere;
    		}
    		*/ 
			
    		if (wh->DataBlocID == DATA_TAG)
    		{
    		    /* standard header (44 bytes) */
    		    data_size = LE2BE_LA(wh->DataSize);
    		    if (opt_verbose>=9) printf("DataSize (Standard Header): %u\n", data_size);
    		    pcm_data_start=&wh->SampleData;    
    		}
    		else
    		{
    		    /* not a standard header => search for data tag */
    		    data_tag=SearchDataTag(&wh->DataBlocID);
    		    if (opt_verbose>=9) printf("Data Tag found: %p\n", data_tag);
    		    data_size=LE2BE_LA(data_tag[1]);
    		    if (opt_verbose>=9) printf("DataSize (Data Tag): %u\n", data_size);
    		    pcm_data_start=(UWORD*)&data_tag[2];
    		}
    		break;
			
		case STREAM_TYPE_AIFF :
			
			if (opt_verbose>=9) printf("Analyze AIFF ...\n");
			commck=SearchChunk((UWORD*)buffer, COMM_TAG); //0x434f4d4d/*"COMM"*/);
			sdc = SearchChunk((UWORD*)buffer, SSND_TAG); //0x53534e44/*"SSND"*/);
			if ((commck) && (sdc))
			{
				data_size = size - (ULONG)((UBYTE*)&sdc->SampleData - (UBYTE*)buffer);
				if (opt_verbose>=9) printf("COMM: %p\n", commck);
				if (opt_verbose>=9) printf("NumChannels: %u\n", commck->NumChannels);
				if (opt_verbose>=9) printf("NumSampleFrames: %u\n", commck->NumSampleFrames);
				if (opt_verbose>=9) printf("Calculated data_size: %u\n", data_size);
				if (opt_verbose>=9) printf("SampleRate: %u\n", commck->SampleRate);
				if (opt_verbose>=9) printf("SSND: %p\n", sdc);
				if (opt_verbose>=9) printf("Offset: %u\n",sdc->Offset);
				if (opt_verbose>=9) printf("BlockSize: %u\n", sdc->BlockSize);
				if (opt_verbose>=9) printf("SampleData: %p\n", &sdc->SampleData);
				pcm_data_start=(UWORD*)&sdc->SampleData;
				//data_size=2000000;
				
				opt_sample_rate=commck->SampleRate;
				
				/*
				if (commck->SampleRate != 44100)
    			{
    			    printf("Frequency (%u) is not supported.\n", LE2BE_LA(wh->Frequency));
    			    goto outahere;
    			}
    			*/
				
			} else goto outahere; 
			
			break;
	}		
			
    cl_ab=cl_bs=0;
    ms_song_duration= (double)data_size / 4.0 / 44.100;
    if (opt_verbose>=9) printf("Length: %u secs\n", ms_song_duration/1000);
  
    if (type == STREAM_TYPE_WAV) BUFFER_LE2BE_LONG((WORD*)pcm_data_start, (first_block >> 2) - 1);
    PlayArneRAW((UWORD*)pcm_data_start, data_size >> 2, opt_volume); 
    
	/* read rest of the file 
	 * (subdivide it in different blocks - sd cards are too slow to read
	 * the rest of the file in one block while prebuffered data is playing)
	 */
	 
	act_block=first_block;
	done=0;
	
	while ((act_block<size) && (!done))
	{ 
		t=fread(buffer+act_block,1,block_size,fh);
		counter = (t >> 2) - 1;
		if (type == STREAM_TYPE_WAV) BUFFER_LE2BE_LONG((WORD*)(pcm_data_start+(act_block>>1)), counter);
		act_block+=block_size;
		
		e=GetAndHandleNotIntEvents(1); 
        switch (e)
        {
            case EVENT_BREAK : 
            case EVENT_NEXT_SONG : 
            case EVENT_PREVIOUS_SONG : done=1; break;
        }
	}
	
	if (done) goto outahere;
	
   	if (!cl_bs)
    {
        PlayArneRAW(pcm_data_start, data_size >> 2, opt_volume);
        cl_bs=clock();
		E_Freq=ReadEClock(&ecl_bs);
    }

    if (opt_ARNE_main>15)
    {
        printf("No free ARNE channels.\n");
        e=EVENT_BREAK;
        in_filename=NULL;
        goto outahere;
    } else if (opt_verbose>=9) printf("Use ARNE channel %u\n", opt_ARNE_main);
    
    done=0;
    do
    {
        if ((GetActualPlayPosition()) >= ms_song_duration) done=1;
        else 
        {
            e = GetAndHandleNotIntEvents(1);
            switch (e)
            {
                case EVENT_BREAK :
                case EVENT_NEXT_SONG : 
                case EVENT_PREVIOUS_SONG : done=1; break;
                
                case EVENT_EXECUTE_SEEK : EnterNormal();
										  printf("\r->| %3.2f secs\n", (double)ctrl_seek/1000.0);
										  pcm_seek_start = PrepareNewSeekDestinationWAV(pcm_data_start, ctrl_seek); 
										  //printf("ctrl_seek: %u\n", ctrl_seek);
                        	              if (pcm_seek_start)
                                          {			  
                                                stopplaying(opt_ARNE_main);
                                                PlayArneRAW(pcm_seek_start, (data_size - MS2SBO(/*pcm_seek_start*/ ctrl_seek)) >> 2, opt_volume);
                                                cl_bs=clock();
												E_Freq=ReadEClock(&ecl_bs);
												if (opt_verbose>=9) printf("data_size: %u ctrl_seek: %u MS2SBO: %u\n", data_size, ctrl_seek, MS2SBO(ctrl_seek));
												if (opt_verbose>=9) printf("pcm_seek_start: %p size rest: %u\n", data_size-MS2SBO(ctrl_seek));
                                          }
                                          break;
										  
				case EVENT_EXECUTE_VOLUME : 
											EnterNormal();
											opt_volume=(UWORD)ctrl_volume | (UWORD)(ctrl_volume & 0xff)<<8;
											printf("\r-/+ %u%\n", (ULONG)((100.0 / 255.0) * (double)ctrl_volume));
											ctrl_seek=GetActualPlayPosition(); // - 50; // + cl_volume_change_delta; //2250;
											pcm_seek_start = PrepareNewSeekDestinationWAV(pcm_data_start,ctrl_seek);
											//printf("execute volume ... ctrl_seek: %u\n", ctrl_seek);
											if (pcm_seek_start)
											{
												stopplaying(opt_ARNE_main);
                                                PlayArneRAW(pcm_seek_start, (data_size - MS2SBO(/*pcm_seek_start*/ ctrl_seek)) >> 2, opt_volume);
                                                cl_bs=clock();
												E_Freq=ReadEClock(&ecl_bs);
												if (opt_verbose>=9) printf("pcm_seek_start: %p size rest: %u\n", data_size-MS2SBO(ctrl_seek));
											}
											break;	
                                        
            }
            Delay(10);
        }
        //printf("e: %u\n", e);
        
    } while (!done);
    
outahere:  
    
	stopplaying(opt_ARNE_main); 
    
    FreeVec(buffer);
    fclose(fh);   
    
	return e;
}