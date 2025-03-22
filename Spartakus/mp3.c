
#include "mp3.h"
#include "events.h"

struct Library *MPEGABase = NULL;
char mpega_name[] = "mpega.library";
char *mpega_name_ptr=mpega_name;

int frame = 0;
LONG pcm_count, total_pcm = 0;
LONG index;
WORD *pcm[ MPEGA_MAX_CHANNELS ];
int custom = 0;                             /* use default bitstream */
long br_sum = 0;
WORD* myPCM=NULL;

MPEGA_STREAM *mps = NULL;

BYTE   *mpega_buffer = NULL;
ULONG  mpega_buffer_offset = 0;
ULONG  mpega_buffer_size = 0;

static const char *layer_name[] = { "?", "I", "II", "III" };
static const char *mode_name[] = { "stereo", "j-stereo", "dual", "mono" };

MPEGA_CTRL mpa_ctrl = {
      NULL,    /* Bitstream access is default file I/O */
      /* Layers I & II settings (mono, stereo) */
      { FALSE, { 1, 2, 44100 }, { 1, 2, 44100 } },
      /* Layer III settings (mono, stereo) */
      { FALSE, { 1, 2, 44100 }, { 1,2, 44100 } },
      0,           /* Don't check mpeg validity at start (needed for mux stream) */
      32678        /* Stream Buffer size */
   };


/* bitstream access */

static ULONG __saveds __asm def_baccess( register __a0 struct Hook  *hook,
                                         register __a2 APTR          handle,
                                         register __a1 MPEGA_ACCESS *access ) {
   //printf("def_baccess(): access->func: %u\n", access->func);
   //printf("OPEN: %u\nCLOSE: %u\nREAD: %u\nSEEK: %u\n",
   //		MPEGA_BSFUNC_OPEN, MPEGA_BSFUNC_CLOSE, MPEGA_BSFUNC_READ, MPEGA_BSFUNC_SEEK); 
   
   switch( access->func ) {

      case MPEGA_BSFUNC_OPEN:
         /* We don't really need stream_name */
         /* buffer_size indicates the following read access size */
         if (opt_verbose>=8) printf( "bitstream open: filename='%s'\n", access->data.open.stream_name );
         if (opt_verbose>=8) printf( "bitstream open: buffer_size=%d\n", access->data.open.buffer_size );

         /* open errors */
         if( !mpega_buffer ) return NULL;

         /* initialize some variables */
         mpega_buffer_offset = 0;

         /* We know total size, we can set it */
         access->data.open.stream_size = mpega_buffer_size;

         /* Just return a dummy handle (not NULL) */
         return 1;

      case MPEGA_BSFUNC_CLOSE:
         if( handle ) {
            /* Clean up */
            if (opt_verbose>=8) printf( "bitstream close\n" );
         }
         break;
      case MPEGA_BSFUNC_READ: {
         LONG read_size;

         if( !handle ) return 0; /* Check valid handle */

         read_size = mpega_buffer_size - mpega_buffer_offset;
         if( read_size > access->data.read.num_bytes ) read_size = access->data.read.num_bytes;

         if( read_size > 0 ) {
            if( !access->data.read.buffer ) return 0;
            /* Fill buffer with our MPEG audio data */
            memcpy( access->data.read.buffer, &mpega_buffer[ mpega_buffer_offset ], read_size );
            mpega_buffer_offset += read_size;
         }
         else {
            read_size = 0; /* End of stream */
         }
         if (opt_verbose>=8) printf( "bitstream read: requested %d bytes, read %d\n", access->data.read.num_bytes, read_size );

         return (ULONG)read_size;
      	}
		break;
		
      case MPEGA_BSFUNC_SEEK:
         if( !handle ) return 0;

         if (opt_verbose>=8) printf( "bitstream seek: pos = %d\n", access->data.seek.abs_byte_seek_pos );

         if( access->data.seek.abs_byte_seek_pos <= 0 ) mpega_buffer_offset = 0;
         else if( access->data.seek.abs_byte_seek_pos >= mpega_buffer_size ) return 1;
         else mpega_buffer_offset = access->data.seek.abs_byte_seek_pos;
         return 0;
   }
   return 0;
}

static struct Hook def_bsaccess_hook = {
   { NULL, NULL }, def_baccess, NULL, NULL
};

/* create pcm data for ARNE (from decoded data) */

WORD *pcmLR=NULL;
WORD *pcm_buffer = NULL;
WORD *pcm_end;

WORD* create_pcm( WORD channels, WORD *pcm[ 2 ], LONG count)
{
   if( !pcm_buffer ) {
      pcm_buffer = (WORD *)AllocVec( PCM_BUFFER_SIZE, MEMF_FAST );  /* ring buffer */
      if( !pcm_buffer ) return (WORD*)(UWORD)-1;
      pcmLR = pcm_buffer;
      pcm_end = pcm_buffer + (2 * FIX_TOTAL_PCM_LENGTH); /* total size = pcm_buffer + FIX_TOTAL_PCM_LENGTH * 2 * 3 bytes (2'824'704 bytes) */
   }
   
   if (pcm)
   {
     if( channels == 2 ) {
        register WORD *pcm0, *pcm1;
        register LONG i;

        pcm0 = pcm[ 0 ];
        pcm1 = pcm[ 1 ];
        i = count;
        while( i-- ) {
            *pcmLR++ = *pcm0++;
            *pcmLR++ = *pcm1++;
            if (pcmLR==pcm_end) pcmLR=pcm_buffer; /* wrap around for ring buffer */
        }
     }
   }
   else
   {
        /* pcm == NULL => fill buffer with 0es at the end */
        while( count-- ) {
            printf("count: %u\n", count);
            *pcmLR++ = 0;
            *pcmLR++ = 0;
            if (pcmLR==pcm_end) pcmLR=pcm_buffer; /* wrap around for ring buffer */
        	if (isIntEvent(GetAndHandleNotIntEvents(0))) return pcm_buffer;
		}
   }
   //if (opt_verbose>=8) printf("pcm_buffer: %p pcmLR: %p (difference: %u)\n", pcm_buffer, pcmLR, (ULONG)pcmLR-(ULONG)pcm_buffer);
   return pcm_buffer;

}

void free_pcm(WORD* pcm_buffer)
{
    FreeVec(pcm_buffer);
}

/*
void AddEmptyPCM(void)
{
	LONG count = 706000 / 8; // about 2 seconds of silence
	while( count-- ) 
	{
        *pcmLR++ = 0;
        *pcmLR++ = 0;
        if (pcmLR==pcm_end) pcmLR=pcm_buffer; // wrap around for ring buffer 
        if (isIntEvent(GetAndHandleNotIntEvents(0))) return pcm_buffer;
	}
}  
*/

UBYTE DecodeMPEGA2PCM(ULONG ms_limit)
{
	UBYTE e = EVENT_NOTHING;
	UBYTE stream_done = FALSE;
	
    while( (!stream_done) && (isNotIntEvent(e)) && (ms<=ms_limit)) 
    { 
    	pcm_count = MPEGA_decode_frame( mps, pcm );
			
        if (pcm_count >= (LONG)0)
        {
        	MPEGA_time( mps, &ms );
            br_sum += mps->bitrate; // #3
            total_pcm += pcm_count;
			
			/* create playable audio (pcm) */
            myPCM=create_pcm(mps->dec_channels, pcm, pcm_count); 
            
			frame++;
            if (((frame & 31) == 0) && (opt_verbose>=4))
            {
				printf( "{%04d} %7.3fkbps\r", frame, (double)br_sum / (double)(frame+1) ); fflush( stderr );
            }
		}
		else stream_done=EVENT_STREAM_DONE;
		
		if (!stream_done) e = GetAndHandleNotIntEvents(1);
		else e = EVENT_STREAM_DONE;
		//printf("e: %u\n", e);
	}
	
	if (!e) Delay(50);
	
	return e;
}  

UBYTE PlayMP3(char* filename)
{
	int i, j, temp;

	MPEGABase = OpenLibrary( mpega_name_ptr, 0L );
   	if( !MPEGABase ) {
    	if (opt_verbose>=8) printf( "Unable to open '%s'\n", mpega_name_ptr );
    	exit( 0 );
	}
	
	for( i=0; i<MPEGA_MAX_CHANNELS; i++ ) 
    {
        pcm[ i ] = malloc( MPEGA_PCM_SIZE * sizeof( WORD ) );
        if( !pcm[ i ] ) 
        {
            if (opt_verbose>=8) printf("Can't allocate PCM buffers\n" );
            exit( 0 );
        }
    }

	if( custom ) 
        { 
            /* custom bitstream access (not used) */
            if (opt_verbose>=8) printf( "Custom bitstream access used\n" ); /* #2 */
            if (!mpega_buffer) mpega_buffer = (BYTE *)malloc( MPEGA_BUFFER_SIZE );
            if( !mpega_buffer ) 
            {
               if (opt_verbose>=8) printf("Can't allocate MPEG buffer\n");
               exit( 0 );
            }

            /* Load the stream into a ram buffer */
            in_file = fopen( in_filename, "rb" );
            if( !in_file ) {
                if (opt_verbose>=8) printf("Unable to open file '%s'\n", in_filename );
                exit( 0 );
            }
            mpega_buffer_size = fread( mpega_buffer, 1, MPEGA_BUFFER_SIZE, in_file );
            fclose( in_file );
            if (opt_verbose>=8) printf( "Read %d bytes from file '%s'\n", mpega_buffer_size, in_filename );
    
            /* #1 Begin */
            if (opt_verbose>=8) printf( "Test if MPEG Audio sync inside...\n" );
            index = MPEGA_find_sync( mpega_buffer, MPEGA_BUFFER_SIZE );
            if( index >= 0 ) 
            {
               if (opt_verbose>=8) printf( "Ok, found MPEG Audio sync at position %d\n", index );
            }
            else 
            {
               if (opt_verbose>=8) printf( "* Error %d *\n", index );
            }
            /* #1 End */
        
            /* Set our bitstream access routines and open the stream */
            mpa_ctrl.bs_access = &def_bsaccess_hook;
        }
        else 
        {
            if (opt_verbose>=8) printf( "Default bitstream access used\n" ); /* #2 */
            if (opt_verbose>=8) printf( "Buffer size = %d\n", mpa_ctrl.stream_buffer_size );
        }

        mps = MPEGA_open( FullFilename(in_filename), &mpa_ctrl );
        if( !mps ) 
        {
			//printf("mps: %p FullFilename(): >%s<\n", mps, FullFilename(in_filename));
            if (opt_verbose>=1) printf( "Unable to find MPEG Audio stream in file '%s'\n", in_filename );
            if (dirplay)
            {
                if (opt_verbose>=1) printf( "Skip file ...\n");
                next_title = opt_reverse ? -1 : +1;
                goto outahere; //terminate;
            }
            else exit( 0 );
        }
          
        if (opt_verbose>=3) 
        {
            printf( "\n" );
            printf( "MPEG norm %d Layer %s\n", mps->norm, layer_name[ mps->layer ] );
            printf( "Bitrate: %d kbps\n", mps->bitrate );
            printf( "Frequency: %d Hz\n", mps->frequency );
            printf( "Mode: %d (%s)\n", mps->mode, mode_name[ mps->mode ] );
            printf( "Stream duration: %ld ms\n", mps->ms_duration );
            printf( "\n" );
            printf( "Output decoding parameters\n" );
            printf( "Channels: %d\n", mps->dec_channels );
            printf( "Quality: %d\n", mps->dec_quality );
            printf( "Frequency: %d Hz\n", mps->dec_frequency );
            printf( "\n" );
        }
		ms_song_duration = mps->ms_duration;
		opt_sample_rate=mps->frequency;
		
		
reseek:
		cl_vs = clock();
        ms=act_seek_ms;
        cl_bs=clock();
		E_Freq=ReadEClock(&ecl_bs);
        ms_bs=act_seek_ms;
        ms_ab=ms_bs;
        ms_be=ms_bs+16000;     /* not a precise value, but ok to intialize playing of s0 (first slot) with that */
        ts=0;

        if (act_seek_ms) 
        {
            if (MPEGA_seek(mps, act_seek_ms)!=0)
            {
                if (opt_verbose>=8) printf("MPEGA_seek error\n");
                act_seek_ms=0;
            }
            else
            {
                MPEGA_time( mps, &ms );
                act_seek_ms = ms;
            }
        }
          
        /*** fill s0 (first slot) and start playing ***/
        if (opt_verbose>=4) printf("Decode slot 0 ...\n");
        pcmLR=pcm_buffer;
   
        if (opt_verbose>=8) printf("ms_bs: %u ms_be: %u ms_ab: %u\n", ms_bs, ms_be, ms_ab);
  
  		temp=DecodeMPEGA2PCM(ms_bs+4000);
		
		stopplaying(opt_ARNE_main);
    
        /* start playing slot 0 */
        j=0;
        do 
        {
            i=playstereo((WORD*)myPCM, FIX_TOTAL_PCM_LENGTH, opt_volume, opt_ARNE_main, opt_ARNE_mode, CalcAUDPER(opt_sample_rate));
            
			
            if (i==0xff)
            {
                /* channel not free */
                if (!j)
                {
                    /* first timy it happens => start search at channel 4 */
                    opt_ARNE_main=4;
                    j=1;
                }
                else
                {
                    /* if channel 4 is not free neither try +1 up to available 15 channels */    
                    opt_ARNE_main++;    
                }
            }
			else 
			{
				/* channel free => start timers */
				cl_bs=clock();
            	E_Freq=ReadEClock(&ecl_bs);
			}
        } while ((i==0xff) && (opt_ARNE_main<=15));
        
        opt_ARNE_main=i;
        
        if (opt_ARNE_main>15)
        {
            printf("No free ARNE channels.\n");
            in_filename=NULL;
			loop_event=EVENT_BREAK;
            goto outahere; //stopimmediately;
        } else if (opt_verbose>=9) printf("Use ARNE channel %u\n", opt_ARNE_main);
		cl_ve=clock();
		
		cl_volume_change_delta = cl_ve - cl_vs; /* delta for volume change */
		
        /*** now fill slots 1-3 ***/
        if (opt_verbose>=4) printf("Decode slots 1-3 ... \n");
		
		loop_event=DecodeMPEGA2PCM(ms_bs+16000);
		
        /* old */
		delta_ms=next_ms=ms;
        if (opt_verbose>=8) printf("Length slots 0-3: %u\n", total_pcm);

		/* handle the interrupting events */
		if (isIntEvent(loop_event)) goto handleint;
		    
        total_pcm=0;
   
        /*** now loop and fill slots periodically (as a ring buffer) ***/
        ts=0;
        sig_check_counter=0;
		
        do
        {
            ms_ab=GetActualPlayPosition();
            
            /* begin / end of slot in ms */
            tstart=ms_bs+ts*4000;
            tend=tstart+4000;
        
            /* whenever next block is free => fill it */
            if (!((tstart < ms_ab) && (ms_ab < tend)))
            {
                /* target slot (ts) is free */
                if (opt_verbose>=4) printf("Decode slot %u (ms: %u ms_ab: %u tstart: %u, tend: %u)\n", ts, ms, ms_ab, tstart, tend);
                
				loop_event=DecodeMPEGA2PCM(tend+16000);
					
                /* point to next slot */ 
                ts+=1;
                
                /* wrap around after 3 (= ring buffer) */
                if (ts>3) 
                { 
                    /* set everything to beginning of pcm_buffer */
                    ts=0;
                    total_pcm=0;
                    ms_bs=next_ms;
                    ms_be=ms_bs+16000;
                    delta_ms=ms-ms_bs;
                    next_ms=ms;
                }
            }
			
        } while ((pcm_count >= 0) && (!isIntEvent(loop_event))); 

handleint:   

   		/* handle the interrupting events */
		if (isIntEvent(loop_event))
		{
   			switch(loop_event)
			{
				case EVENT_BREAK : in_filename=0;
								   goto outahere; //stopimmediately;
								   break; 	
								   
				case EVENT_NEXT_SONG :
				case EVENT_PREVIOUS_SONG : goto outahere; //terminate;
										   break;
										   
				case EVENT_EXECUTE_SEEK	: EnterNormal();
										  printf("\r->| %3.2f secs\n", (double)ctrl_seek/1000.0);
										  PrepareNewSeekDestination(ctrl_seek); 
                        				  stopplaying(opt_ARNE_main);
										  goto reseek;  
										  break;
										  
				case EVENT_EXECUTE_VOLUME : EnterNormal();
											opt_volume=(UWORD)ctrl_volume | (UWORD)(ctrl_volume & 0xff)<<8;
											printf("\r-/+ %u%\n", (ULONG)((100.0 / 255.0) * (double)ctrl_volume));
											ctrl_seek=GetActualPlayPosition() + cl_volume_change_delta; //2250;
											if (ctrl_seek<mps->ms_duration)
											{
												PrepareNewSeekDestination(ctrl_seek);
												goto reseek;
											}
											break;
			}
		}
		
//   if (opt_verbose>=8) printf( "\ntime used = %7.3f secs\n", secs ); // #1
//   if (opt_verbose>=8) printf( "%ld samples / sec\n", (int)((double)total_pcm / secs) );
//   if (opt_verbose>=8) printf( "%7.3f % CPU used on real time\n", ((double)mps->frequency * 100) / ((double)total_pcm / secs) );

        if (opt_verbose>=8) printf( "\n" );
        if (opt_verbose>=8) printf( "last pcm_count = %d\n", pcm_count );
        if (opt_verbose>=8) printf( "total_pcm = %d\n", total_pcm );

        next_title = opt_reverse ? -1 : +1;
		
terminatewithwait: 

        /* actual play position in ms */
        ms_ab=GetActualPlayPosition();
        
        /* end not yet reached => continue to play last slot */
        if (ms_ab >= ms_song_duration-1000) 
        {   
            ms_ab = ms_song_duration; /* cheat a bit at the end ... */
        }
		
        /* show play position */
        if (((opt_verbose==2) || (opt_verbose==3) || (opt_verbose==9)) && (jump_activated==0))
        {
            printf("\r[%u/%us]", ms_ab/1000, ms_song_duration/1000);
            fflush(stdout);
        }
		
		ms_ab=GetActualPlayPosition();
        
        /* end reached? */
        if (ms_ab + 1500 /*+ 1000*/ >= ms_song_duration) goto outahere; //terminate;
        
        if (ms_ab + 2000 < ms_song_duration) Delay(2);
         
        goto terminatewithwait;

outahere:
		if (mps) MPEGA_close( mps );
        mps=NULL;
				
		CloseLibrary( MPEGABase );
    	//RemLibrary(MPEGABase);
		
		MPEGABase = NULL;
		
		free_pcm(myPCM);
		
		return loop_event;
   
}