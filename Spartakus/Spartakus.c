/*---------------------------------------------------------------------------------

 
    Files   :   Spartakus.c / Spartakus_asm.s

    Author  :   RedBug (MP3 decoding based on MPEGA_demo.c by Stéphane TAVENARD)

    Date    :   January 2025
    
    Description :

    A minimalistic (Spartan) mp3-player
    
    Plays an mp3 files in CD Quality from command line (aka Shell aka CLI).
    Spartan uses the SAGA chipset (ARNE 16 bit audio) directly, playing music
    data via a ring buffer (so, no interrupts are used). 
    
    Some minimalistic CTRL commands have been implemented:
    
    - CTRL-C:   Interrupt playing
    - CTRL-D:   Play mode: Play previous song in list
                Seek mode: Jump forwards inside song
    - CTRL-F:   Play mode: Play next song in list
                Seek mode: Jump backwards inside song
    - CTRL-E:   Toggle seek mode / play mode
    
    The following options can be set via command line:
    
    ?           Help
    -?          Help
    
    -a=x        ARNE main channel
    -b=x        ARNE secondary channel
    -d=x        Alternative decoder library (default: mpega.library)
    -i          Jump backwards (in ms)
    -j          Jump forwards (in ms)
    -l          Loop (repeat playlist or song endlessly)
    -o=x        Volume (0-255, default 0xf0)
    -p=x        Play song with this number (in list)
    -r          Reverse playing order (backwards instead of forwards)
    -s=x        Seek position in ms (playing starts at position x)
    -t=x        Max titles (buffer size for playlist)
    -q=x        Quality (2=best, 1=medium, 0=low)
    -v=x        Verbose level x (9=highest, 0=off)
    
    Usage:
    
    Spartakus [file|drawer] [options]
    
    When a drawer is given, Spartakus plays all mp3 inside the drawer in
    alphabetical order. When no argument is given, Spartakus plays from the
    current drawer.
    
    Spirit:
    
    Get rid of the unnecessary - reduce to the max!

    LICENSE
    
    Copyright (C) 2025  RedBug aka Marcel Maci, Rue des Augustins 9, 
    1700 Fribourg, m.maci at gmx.ch

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  
    
    If not, see <https://www.gnu.org/licenses/>.

    
---------------------------------------------------------------------------------*/
/* defines */
#include "defines.h"

/* includes */
#include <exec/exec.h>
#include <clib/exec_protos.h>
#include <pragmas/exec_pragmas.h>
#include <dos.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <proto/timer.h>
#include <devices/timer.h>
/*
#include <libraries/mpega.h>
#include <clib/mpega_protos.h>
#include <pragmas/mpega_pragmas.h>
*/
#include "Spartakus.h"
#include "mp3.h"
//#include "flac_tools.h"

/* asm prototypes */
UBYTE __asm playstereo(
	register __a0 WORD*,	/* data ptr */ 
	register __d0 ULONG,    /* length */ 
	register __d1 UWORD,    /* volume */
	register __d2 UBYTE,	/* channel */
	register __d5 WORD,		/* mode */
	register __d6 WORD		/* period (frequency) */
	);
void __asm stopplaying(register __d0 UBYTE);
void __asm waitmouse(void);
UBYTE __asm mousedown(void);
UWORD __asm detectvamp(void);
void __asm BUFFER_LE2BE_LONG(register __a0 WORD*, register __d0 ULONG);
UWORD __asm LE2BE_WA(register __d0 UWORD);
ULONG __asm LE2BE_LA(register __d0 ULONG);

/* version */
static char *Version = "$VER:Spartakus - Version: 25.3 R22\nA minimalistic music player\n(CopyLeft) RedBug";

/* libraries */
extern struct Library *DOSBase;
struct Library *TimerBase;
/*
struct Library *MPEGABase = NULL;
char mpega_name[] = "mpega.library";
char *mpega_name_ptr=mpega_name;
*/

/* global variables */

/*
MPEGA_STREAM *mps = NULL;

BYTE   *mpega_buffer = NULL;
ULONG  mpega_buffer_offset = 0;
ULONG  mpega_buffer_size = 0;
*/

/*int frame = 0;
LONG pcm_count, total_pcm = 0;
LONG index;
WORD *pcm[ MPEGA_MAX_CHANNELS ];
int custom = 0;                             /* use default bitstream */
/*long br_sum = 0;
WORD* myPCM=NULL;
*/

char *main_argument=NULL;
char *path=NULL;
char *in_filename=NULL;
char *fullfilename[300];
FILE *in_file;                              /* for custom bitstream (not used) */
char *filenames_buffer;
char **filenames_list;
char *act_filenames_ptr;

UBYTE dirplay=0;
WORD title_count=0;
WORD act_title=0;
int next_title=1;
UWORD opt_start_title=0xffff;               /* ffff = invalid option */

UBYTE alphabetical=1;

clock_t clk; 
clock_t clk_play_start;
clock_t clk_play_now;
clock_t cl_bs=0, cl_ab=0;                   /* bs = buffer start, ab = ARNE beam */
LONG cl_delta=0;

ULONG ms=0;
ULONG last_ms, next_ms, delta_ms;
ULONG ms_bs, ms_be, ms_ab;                  /* milliseconds: be=buffer start, be=buffer end, ab=ARNE beam */
double secs; 
LONG jump_dest=0;
UBYTE jump_activated=0;
ULONG cumulative_seek_ms=0;
ULONG act_seek_ms;
UWORD new_volume=0;
ULONG ms_song_duration;

struct IORequest* TimerIO;
struct EClockVal ecl_bs, ecl_ab;
ULONG E_Freq;
ULONG timer_error;

UWORD ts;                                   /* target slot (0-3) */
ULONG tstart, tend;                         /* temp vars: temp start, temp end */

ULONG signals;
ULONG sig_check_counter=0;
BPTR lock=NULL;
struct FileInfoBlock *fib;

UBYTE stream_done;
//UBYTE signal_done;
UBYTE loop_event;

/* options */

UBYTE opt_verbose=2;                    /* -v */
UBYTE opt_loop=0;                       /* -l */
UBYTE opt_reverse=0;                    /* -r */
ULONG opt_seek_ms=0;                    /* -s */
UWORD opt_max_titles=MAX_TITLES;        /* -t */
UBYTE opt_quality=2;                    /* -q */
UWORD opt_volume=0xf0f0;                /* -o */
UBYTE opt_ARNE_main=7;                  /* -a */
UBYTE opt_ARNE_secondary=8;             /* -b */
LONG opt_jump_bkw_ms=25000;            /* -i */
LONG opt_jump_fwd_ms=20000;            /* -j */
WORD opt_ARNE_mode=ARNE_STEREO|ARNE_LOOP|ARNE_16BIT;
UWORD opt_sample_rate=44100;		

/* functions */

void PrintHelp(void)
{
    printf("\nSpartakus\n---------\nA minimalistic music player\n\n");
    
    printf("Plays (an) mp3/wav/aiff/flac file(s) in from command line (aka Shell/CLI).\n");
    printf("Spartakus uses the SAGA chipset (ARNE 16 bit audio) directly, playing music\n");
    printf("data via a ring buffer (so, no interrupts are used).\n"); 
    printf("\n");
    printf("Some minimalistic CTRL commands have been implemented:\n");
    printf("\n");
    printf("- CTRL-C:   Interrupt playing or cancel values / mode.\n");
    printf("- CTRL-D:   Play mode: Play previous song in list\n");
    printf("            Seek mode: Jump forwards inside song\n");
	printf("            Volume mode: Decrement volume\n");
    printf("- CTRL-F:   Play mode: Play next song in list\n");
    printf("            Seek mode: Jump backwards inside song\n");
	printf("            Volume mode: Increment volumen\n");
    printf("- CTRL-E:   Toggle frot seek mode to play mode to volume mode\n");
    printf("\n");   
    printf("The following options can be used / set via command line:\n");
    printf("\n");
    printf("?           Help\n");
    printf("-?          Help\n");
    printf("\n");
    printf("-a=x        ARNE main channel\n");
    printf("-b=x        ARNE secondary channel (not used atm)\n");
    printf("-d=x        Alternative decoder library (for example MYLIB:mpega.library)\n");
    printf("-i          Jump backwards (in ms, default: 25000)\n");
    printf("-j          Jump forwards (in ms, default: 20000)\n");
    printf("-l          Loop (repeat playlist or song endlessly)\n");
    printf("-o=x        Volume (0-255, default 0xf0)\n");
    printf("-p=x        Play song with this number (in list)\n");
    printf("-r          Reverse playing order (backwards instead of forwards)\n");
    printf("-s=x        Seek position in ms (playing starts at position x)\n");
    printf("-t=x        Max titles (buffer size for playlist)\n");
    printf("-q=x        Quality (2=best, 1=medium, 0=low)\n");
    printf("-v=x        Verbose level x (9=highest, 0=off)\n");
    printf("\n");
    printf("Usage:\n");
    printf("\n");
    printf("Spartakus [file|drawer] [options]\n");
    printf("\n");
    printf("When a drawer is given, Spartakus plays all music inside the drawer in\n");
    printf("alphabetical order. When no argument is given, Spartakus plays from the\n");
    printf("current drawer.\n");
    printf("\n");
    printf("Sources: https://www.github.com/r3dbug/c/Spartakus\n");
    printf("\n");
    printf("LICENSE\n");
    printf("\n");
    printf("Copyright (C) 2025  RedBug aka Marcel Maci, Rue des Augustins 9, \n");
    printf("1700 Fribourg, m dot maci at gmx.ch\n");
    printf("\n");
    printf("This program is free software: you can redistribute it and/or modify\n");
    printf("it under the terms of the GNU General Public License as published by\n");
    printf("the Free Software Foundation, either version 3 of the License, or\n");
    printf("(at your option) any later version.\n");
    printf("\n");
    printf("This program is distributed in the hope that it will be useful,\n");
    printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    printf("GNU General Public License for more details.\n");
    printf("\n");
    printf("You should have received a copy of the GNU General Public License\n");
    printf("along with this program.  \n");
    printf("\n");
    printf("If not, see <https://www.gnu.org/licenses/>.\n");
    printf("\n");
    printf("(Re-)(Vamped) Amiga(s) rule(z)!\n(Keep the momentum going ... :)\n\n");
    exit(0);
}

void ArgError(char *s)
{
    printf("Argument error: %s\n");
}

void ParseArgs(int argc, char **argv) 
{
    int i;
    
    if (argc <= 1)
    {
        main_argument=NULL;
        if (opt_verbose>=3) printf("No argument given (using current dir).\n");
        return;
    }
    
    for (i=1; i<argc; i++)
    {
        switch (argv[i][0])
        {        
            case '?': PrintHelp(); 
                      break;
            case '-': /* - introduces an option */
                      switch (tolower(argv[i][1]))
                      {
                            case 'a' : /* ARNE main channel */
                                       if (argv[i][2]=='=') opt_ARNE_main=atoi((char const*)argv[i][3]);
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_ARNE_main);
                                       break;
                            case 'b' : /* ARNE secondary channel */
                                       if (argv[i][2]=='=') opt_ARNE_secondary=atoi((char const*)argv[i][3]);
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_ARNE_secondary);
                                       break;
                            case 'd' : /* alternative decoder (mpega.library)y) */
                                       if (argv[i][2]=='=') mpega_name_ptr=&argv[i][3];
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%s\n", argv[i][1], mpega_name_ptr);
                                       break;
                            case 'i' : /* jump backwards (in ms) */
                                       if (argv[i][2]=='=') opt_jump_fwd_ms=atoi((char const*)argv[i][3]);
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_jump_fwd_ms);
                                       break;
                            case 'j' : /* jump forwards (in ms) */
                                       if (argv[i][2]=='=') opt_jump_bkw_ms=atoi((char const*)argv[i][3]);
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_jump_bkw_ms);
                                       break;
                            case 'l' : /* loop (repeat list or song) */
                                       if (argv[i][2]=='=') opt_loop=(UBYTE)(argv[i][3]-'0');
                                       else if (argv[i][2]==0) opt_loop=1; 
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_loop);
                                       break;
                            case 'o' : /* volume */
                                       if (argv[i][2]=='=') opt_volume = atoi(&argv[i][3]);
                                       else ArgError(argv[i]);
                                       if (opt_volume>255) opt_volume=255;
                                       opt_volume = ((opt_volume & 0xff) << 8) | (opt_volume);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_volume);
                                       break;
                            case 'p' : /* play song with this number (in list)  */
                                       if (argv[i][2]=='=') opt_start_title = atoi(&argv[i][3]);
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_start_title);
                                       if (opt_start_title) opt_start_title--;
                                       break;
                            case 'r' : /* reverse (playlist order) */
                                       if (argv[i][2]=='=') opt_reverse=(UBYTE)(argv[i][3]-'0');
                                       else if (argv[i][2]==0) opt_reverse=1;
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_reverse);
                                       break;
                            case 's' : /* seek position (in ms) */
                                       if (argv[i][2]=='=') opt_seek_ms = atoi(&argv[i][3]);
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_seek_ms);
                                       break;
                            case 't' : /* titles (max titles in playlist) */
                                       if (argv[i][2]=='=') opt_max_titles = atoi(&argv[i][3]);
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_max_titles);
                                       break;
                            case 'q' : /* quality (0,1,2=best) */
                                       if (argv[i][2]=='=') opt_quality=(UBYTE)(argv[i][3]-'0');
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: -%c=%u\n", argv[i][1], opt_quality);
                                       mpa_ctrl.layer_1_2.mono.quality=opt_quality;
                                       mpa_ctrl.layer_1_2.stereo.quality=opt_quality;
                                       mpa_ctrl.layer_3.mono.quality=opt_quality;
                                       mpa_ctrl.layer_3.stereo.quality=opt_quality;
                                       break;
                            case 'v' : /* verbose */
                                       if (argv[i][2]=='=') opt_verbose=(UBYTE)(argv[i][3]-'0');
                                       else ArgError(argv[i]);
                                       if (opt_verbose>=9) printf("Option: %c=%u\n", argv[i][1], opt_verbose);
                                       break;
                            case '?' : PrintHelp();
                                       break;
                      }
                      break;
                      
            default : /* file or drawer as main argument */
                      main_argument=argv[i];
                      if (opt_verbose>=9) printf("Argument: %s\n", main_argument);
        }
    }
}

struct IORequest* OpenEClockDevice(void)
{
	ULONG error;
	
	TimerIO = (struct IORequest*)AllocVec(sizeof(struct timerequest), MEMF_ANY);
	if (!(error = OpenDevice(TIMERNAME,UNIT_ECLOCK,TimerIO,0L)) )
    {
          TimerBase = (struct Library *)TimerIO->io_Device;

          E_Freq = ReadEClock(&ecl_ab);   /* Get initial reading */
		  
		  return TimerIO;
    } 
	else return NULL;
}

void CloseEClockDevice(void)
{
    CloseDevice( (struct IORequest *) TimerIO );
	FreeVec(TimerIO);
}

ULONG EClockDiffInMS(struct EClockVal* a, struct EClockVal* b, ULONG E_Freq)
{
	double fa, fb, fd;
	
	fa = (double)a->ev_hi * (double)0xffffffff + (double)a->ev_hi + (double)a->ev_lo;
	fb = (double)b->ev_hi * (double)0xffffffff + (double)b->ev_hi + (double)b->ev_lo;
	fd = fb-fa;
	return (ULONG)(fd/(double)E_Freq * 1000.0 + 0.5);
}

char *FullFilename(char *s)
{
    char *a=(char*)fullfilename;
    if (path) 
    {
        strcpy(a,path);
        a+=strlen(path)-1;
        if (*a != '/') *(++a)='/';
        a++;
    }

    strcpy(a,s);
    return (char*)fullfilename;
}


static int break_cleanup( void )
{
   if( mps ) {
      MPEGA_close( mps );
      mps = NULL;
   }
   if( MPEGABase ) {
      CloseLibrary( MPEGABase );
      MPEGABase = NULL;
   }
   return 1;
}

static void exit_cleanup( void )
{
   (void)break_cleanup();
}
/*
UBYTE DetectArne(void) {
	UWORD value;
	UBYTE* potgor=(UWORD*)0xdff016;
	value=*potgor;
	if (value & 2) return 1; /* ARNE */
//	else return 0; 			 /* Paula */	
//}

UWORD CheckHW(void)
{
    UWORD vampire;
    
    if (vampire=detectvamp()) {
        //if (DetectArne()) {
            return vampire;   
        //} else return 0;
    } else return 0;
}



/* overriding routines (this is specific to SAS/C) */

int CXBRK(void) { return 0; }
void __regargs __chkabort(void) { return; }       /* avoid default ctrl-c break */



/* seek timing */

ULONG ms_offset;
ULONG byte_offset;
UWORD* pcm_ab;    
    
ULONG GetActualPlayPosition(void)
{
	ULONG app; /* Actual Play Position */
	struct EClockVal ecl_ab_temp;
	E_Freq=ReadEClock(&ecl_ab_temp);
	app= EClockDiffInMS(&ecl_bs, &ecl_ab_temp, E_Freq) + act_seek_ms; 
	return app; //+ act_seek_ms;
}

clock_t cl_vs, cl_ve, cl_volume_change_delta;
   
//#include "mp3.c"

UBYTE PrepareNewSeekDestination(LONG delta)
{
	if (opt_verbose>=9) printf("\nNew seek position\n");
	
    jump_dest = delta; 
    
    if ((jump_dest < mps->ms_duration) && (jump_dest >= 0))
    {
        MPEGA_seek(mps, jump_dest);
        MPEGA_time( mps, &ms );

        act_seek_ms=jump_dest;
        stream_done=0;
        cl_bs=0;
        pcmLR=pcm_buffer;
        cumulative_seek_ms=0;
        
        return 1;
		
    } else return 0;
}

UWORD CalcAUDPER(UWORD sr)
{
	/* calculate AUDPER for ARNE from sample rate (sr) / frequency */
	/* examples: 
	 * - 44.1 khz: 80 (80.4)
	 * - 48 khz: 74 (73.8)
	 * - 56 khz: 65 (?)
	 */
	
	// correct value seems to be 3546895 (according to RIVA)
	// this is PAL frequency 7.09379 divided by 2
	//ULONG i=3552000; //3528000; // 3600000; // what's the correct value here?!
	ULONG i=3546895;
	UWORD v=(UWORD)((double)i/(double)sr+0.5);
	
	//printf("SR: %u => WORD: %u\n", sr, v);
	return v;
}

void PlayArneRAW(UWORD* pcm, ULONG size, UWORD volume)
{
    UBYTE i,j=0;
	//printf("PlayArneRAW(pcm: %p, size: %u, volume: 0x%x\n", pcm, size, volume);
	
    do 
    {
        i=playstereo((WORD*)pcm, size, volume, opt_ARNE_main, ARNE_STEREO | ARNE_16BIT | ARNE_ONE_SHOT, CalcAUDPER(opt_sample_rate));
        cl_bs=clock();
        E_Freq=ReadEClock(&ecl_bs);
		
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
    } while ((i==0xff) && (opt_ARNE_main<=15));
        
    opt_ARNE_main=i;
}

void PlayArneRAWLoop(UWORD* pcm, ULONG size, UWORD volume)
{
    UBYTE i,j=0;
	
    do 
    {
        i=playstereo((WORD*)pcm, size, volume, opt_ARNE_main, ARNE_STEREO | ARNE_16BIT | ARNE_LOOP, CalcAUDPER(opt_sample_rate));
        cl_bs=clock();
        E_Freq=ReadEClock(&ecl_bs);
		
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
    } while ((i==0xff) && (opt_ARNE_main<=15));
        
    opt_ARNE_main=i;
}

#include "raw_tools.c"
#include "flac_tools.c"

/* main program */

int main( int argc, char **argv )
{
   WORD i,j;
   ULONG temp;
   char* tchar;
   UWORD vampire;
   
   if (!OpenEClockDevice())
   {
   		printf("Could not open E-Clock-Timer.\n");
		exit(0);
   }
   
   ParseArgs(argc, argv);

   if (opt_verbose>=1) printf("%s\n", &Version[5]);
   
   if (!(vampire=CheckHW()))
   {
        printf("\nARNE 16bit audio chipset required.\n");
        printf("More info: www.apollo-computer.com\n");
        exit(0);
   }
   else
   {
        if (opt_verbose>=9) printf("Vampire card (%u) detected! :)\n", vampire);
   }
   
   onbreak( break_cleanup );
   atexit( exit_cleanup );

   /* old open mpega */

   if (main_argument==NULL) lock=Lock("", ACCESS_READ);
   else lock=Lock(main_argument, ACCESS_READ);

   if (lock)
   {
        if (fib=(struct FileInfoBlock*)AllocDosObjectTags(DOS_FIB, TAG_END))
        {
            if (Examine(lock, fib))
            {
                if (fib->fib_DirEntryType>0)
                {
                    /* file is a directory => use dirplay */
                    dirplay=1;
                        
                    path=main_argument;
                        
                    filenames_buffer = (char*)AllocVec(FILENAMES_BUFFER_SIZE, MEMF_ANY);
                    filenames_list = (char**)AllocVec(opt_max_titles * sizeof(char*), MEMF_ANY);
        
                    for (temp=MAX_TITLES-1; temp; temp--) filenames_list[temp]=NULL;
                    act_filenames_ptr=filenames_buffer;
                    
                    while (ExNext(lock,fib))
                    {
                        if (fib->fib_DirEntryType<0)
                        {
                            //if (opt_verbose>=8) printf("fib_FileName: %s\n", fib->fib_FileName); 
                                
                            /* file is a file (not a directory) */
                            if (
								(strstr(fib->fib_FileName,".mp3")) 
								|| 
								(strstr(fib->fib_FileName,".MP3"))
								|| 
								(strstr(fib->fib_FileName,".wav"))
								|| 
								(strstr(fib->fib_FileName,".WAV"))
								|| 
								(strstr(fib->fib_FileName,".aiff"))
								|| 
								(strstr(fib->fib_FileName,".AIFF"))
								||
								(strstr(fib->fib_FileName,".flac"))
								|| 
								(strstr(fib->fib_FileName,".FLAC"))
							   )
                            {
                                /* file is an mp3 */
                                //if (opt_verbose>=8) printf("MP3: fib_FileName: %s\n", fib->fib_FileName); 
                                    
                                strcpy(act_filenames_ptr, fib->fib_FileName);
                                filenames_list[title_count]=act_filenames_ptr;
                                act_filenames_ptr+=strlen(fib->fib_FileName)+1;
                                title_count++;
                            }
                        }
                    }
                        
                    /* order playlist */
                    if (alphabetical)
                    {
                        for (i=0; i<title_count; i++)
                        {
                            for (j=i; j<title_count; j++)
                            {
                                if (strcmp(filenames_list[i],filenames_list[j])>0) 
                                {
                                    tchar=filenames_list[j];
                                    filenames_list[j]=filenames_list[i];
                                    filenames_list[i]=tchar;
                                }
                            }
                        }
                    }
                        
                    /* show dirplay playlist */
                    if (opt_verbose>=2) 
                    {
                        printf("\nPlaylist [%d songs]:\n", title_count);
                        for (temp=0; temp<title_count; temp++)
                        {
                            printf("%02u: %s\n", temp+1, filenames_list[temp]);
                        }
                        printf("\n");
                    }
                    
                    /* set filename to play */
                    if ((opt_start_title == 0xffff) || (opt_start_title >= title_count))
                    {
                        switch (opt_reverse)
                        {
                            case 0 : in_filename=filenames_list[0]; act_title=0; break;
                            case 1 : in_filename=filenames_list[title_count-1]; act_title=title_count-1; break;
                        }
                    }
                    else
                    {
                        act_title=opt_start_title;
                        in_filename=filenames_list[act_title];
                        opt_start_title=0xffff;
                    }
                }
                else
                {
                    /* file is a file (not a drawer)  => use singleplay */
                    dirplay=0;
                    in_filename = main_argument;
                    if (opt_verbose>=1) printf("\n");
                }
            }
            FreeDosObject(DOS_FIB, fib);
        }
        else if (opt_verbose>=8) printf("AllocDosObjectTags for FileInfoBlock failed.\n");
        UnLock(lock);
    } 
    else if (opt_verbose>=8) printf("Lock not possible.\n");
        
    if (opt_verbose>=2) printf("Play song [%u]:\n%s\n", act_title+1, in_filename); 
    
    while (in_filename) /* dirplay: play as long as there are more files available */
    {
       act_seek_ms=opt_seek_ms;

	   /* WAV */
	   if (
			(strstr(in_filename, ".wav"))
			||
			(strstr(in_filename, ".WAV"))
		   )
	   {
			loop_event=PlayRAW(FullFilename(in_filename), STREAM_TYPE_WAV);
			//printf("\nloop_event: %u\n", loop_event);
			if (loop_event==EVENT_NEXT_SONG) goto terminate;
	   		if (loop_event==EVENT_PREVIOUS_SONG) goto terminate;
			else if (loop_event==EVENT_BREAK) { in_filename = NULL; goto stopimmediately; }
			goto terminate;
	   }

	   /* AIFF */
	   if (
			(strstr(in_filename, ".aiff"))
			||
			(strstr(in_filename, ".AIFF"))
		   )
	   {
			loop_event=PlayRAW(FullFilename(in_filename), STREAM_TYPE_AIFF);
			//printf("\nloop_event: %u\n", loop_event);
			if (loop_event==EVENT_NEXT_SONG) goto terminate;
	   		if (loop_event==EVENT_PREVIOUS_SONG) goto terminate;
			else if (loop_event==EVENT_BREAK) { in_filename = NULL; goto stopimmediately; }
			goto terminate;
	   }
	   
	   /* FLAC */
	   if (
			(strstr(in_filename, ".flac"))
			||
			(strstr(in_filename, ".FLAC"))
		   )
	   {
			loop_event=PlayFLAC(FullFilename(in_filename));
			
			//printf("\nloop_event: %u\n", loop_event);
			if (loop_event==EVENT_NEXT_SONG) goto terminate;
	   		if (loop_event==EVENT_PREVIOUS_SONG) goto terminate;
			else if (loop_event==EVENT_BREAK) { in_filename = NULL; goto stopimmediately; }
			goto terminate;
	   }
	   
       /* MP3 */
	   if (
			(strstr(in_filename, ".mp3"))
			||
			(strstr(in_filename, ".MP3"))
		   )
	   {
			loop_event=PlayMP3(FullFilename(in_filename));
			
			//printf("\nloop_event: %u\n", loop_event);
			if (loop_event==EVENT_NEXT_SONG) goto terminate;
	   		if (loop_event==EVENT_PREVIOUS_SONG) goto terminate;
			else if (loop_event==EVENT_BREAK) { in_filename = NULL; goto stopimmediately; }
			goto terminate;
	   }
   
terminate:

        in_filename=NULL; 
  
        if (dirplay)
        {
            act_title += next_title;
            
            if ((0<=act_title) && (act_title<=title_count)) in_filename=filenames_list[act_title];
            
            if (act_title < 0) 
            {
                if (opt_loop)
                {
                    act_title=title_count-1;
                    in_filename=filenames_list[act_title];
                }
                else if (!opt_reverse)
                {
                    act_title=0;        /* play title 0 again */
                    in_filename=filenames_list[act_title];
                }
                else in_filename=NULL;  /* opt_loop not active && opt_reverse => stop playing */
            }
            
            if (act_title==title_count) 
            {
                if (opt_loop)
                {
                    act_title=0;
                    in_filename=filenames_list[act_title];
                }
                else if (opt_reverse)
                {
                    act_title=act_title-1;
                    in_filename=filenames_list[act_title]; /* play again last title */
                }
                else in_filename=NULL;
            }
            
            if (in_filename) 
            {
                if ((opt_verbose==2) || (opt_verbose==3)) printf("\r               ");
                if (opt_verbose>=2) printf("\nPlay song [%u]:\n%s\n", act_title+1, in_filename); 
            }
        }

stopimmediately:

        total_pcm=0;
        frame=0;
        stopplaying(opt_ARNE_main);
        if (opt_verbose>=9) printf("Stop ARNE channel: %u\n", opt_ARNE_main);
       	
    } /* end dirplay */

    if ((opt_verbose==2) || (opt_verbose==3)) printf("\n");
          
    if (dirplay)
    {
        FreeVec(filenames_buffer);
        FreeVec(filenames_list);
    }
        
	CloseEClockDevice();
}

