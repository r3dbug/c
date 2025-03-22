
#include "events.h"

/* control variables */

UBYTE ctrl_mode=MODE_NORMAL;						
LONG  ctrl_seek=0;
LONG  ctrl_volume=0;
UBYTE ctrl_seek_modified=0;
UBYTE ctrl_volume_modified=0;

UBYTE SeekCtrlE(void)
{
	switch (ctrl_seek_modified)
	{
		case 1 : return EVENT_EXECUTE_SEEK; break;
		case 0 : return EVENT_SWITCH_TO_VOLUME; break;	
	}
}

UBYTE SeekCtrlC(void)
{
	switch (ctrl_seek_modified)
	{
		case 1 : return EVENT_SEEK_CANCEL; break;
		case 0 : return EVENT_SWITCH_TO_NORMAL; break;	
	}
}

UBYTE VolumeCtrlE(void)
{
	switch (ctrl_volume_modified)
	{
		case 1 : return EVENT_EXECUTE_VOLUME; break;
		case 0 : return EVENT_SWITCH_TO_NORMAL; break;	
	}
}

UBYTE VolumeCtrlC(void)
{
	switch (ctrl_volume_modified)
	{
		case 1 : return EVENT_VOLUME_CANCEL; break;
		case 0 : return EVENT_SWITCH_TO_NORMAL; break;	
	}
}

void UpdateSeek(LONG s)
{
	ctrl_seek_modified = 1;
	ctrl_seek = (LONG)ctrl_seek + s;
	if (ctrl_seek>ms_song_duration) ctrl_seek=ms_song_duration;
	else if (ctrl_seek<0) ctrl_seek=0;
	printf("\r             \r-> %3.2f secs", (double)ctrl_seek/1000.0);
	fflush(stdout);	
}

void UpdateVolume(LONG v)
{
	ctrl_volume_modified = 1;
	ctrl_volume = (LONG)ctrl_volume + v;
	if (ctrl_volume>255) ctrl_volume=255;
	else if (ctrl_volume<0) ctrl_volume=0;
	printf("\r                 \r-/+ %u%", (ULONG)((100.0 / 255.0) * (double)ctrl_volume));
	fflush(stdout);	
}

void EnterSeek(void)
{
	ctrl_seek = GetActualPlayPosition();
	ctrl_seek_modified = 0;
	ctrl_mode = MODE_SEEK;
	printf("\r                      \r-> Seek");
	fflush(stdout);	
}

void EnterVolume(void)
{
	ctrl_volume = (UBYTE)(opt_volume & 0xff);
	ctrl_volume_modified = 0;
	ctrl_mode = MODE_VOLUME;
	printf("\r                      \r-> Volume");
	fflush(stdout);	
}

void EnterNormal(void)
{
	ctrl_mode = MODE_NORMAL;
	printf("\r               \r");
	fflush(stdout);	
}

void UpdateNormal(void)
{
	/* show play position */
	switch (opt_verbose)
    {
		case 2 :
		case 3 : ms_ab=GetActualPlayPosition();
				 if ((cl_bs) && (ms_ab >= 4000))
				 {
				 	printf("\r[%u/%us]", ms_ab/1000, ms_song_duration/1000);
        		 	fflush(stdout);
    			 }
				 break;
	}
}

void HandleEventVarsAndPrints(UBYTE e)
{
	switch (e)
	{
		case EVENT_NEXT_SONG : next_title = opt_reverse ? -1 : +1; break;
		case EVENT_PREVIOUS_SONG : next_title = opt_reverse ? +1 : -1; break;
		case EVENT_SWITCH_TO_SEEK : EnterSeek(); break;
		case EVENT_SWITCH_TO_VOLUME	: EnterVolume(); break;
		case EVENT_SWITCH_TO_NORMAL : EnterNormal(); break;
		case EVENT_INCREMENT_SEEK : UpdateSeek(opt_jump_fwd_ms); break;
		case EVENT_DECREMENT_SEEK : UpdateSeek(-opt_jump_bkw_ms); break;
		case EVENT_INCREMENT_VOLUME	: UpdateVolume(+20); break;
		case EVENT_DECREMENT_VOLUME	: UpdateVolume(-20); break;
		case EVENT_SEEK_CANCEL : EnterSeek(); break;
		case EVENT_VOLUME_CANCEL : EnterVolume();  break;

	}
	
	if (ctrl_mode == MODE_NORMAL) UpdateNormal();
}

UBYTE GetEvent(UBYTE clr)
{
	ULONG signals = SetSignal(0,0);
	
	if (signals)
	{
		/* clear signal */
		if (clr) SetSignal(0,signals); 
	
    	/* CTRL commands depend on the mode */
		switch (ctrl_mode)
		{
			case MODE_NORMAL : if (signals & SIGBREAKF_CTRL_D) return EVENT_PREVIOUS_SONG;
							   else if (signals & SIGBREAKF_CTRL_F) return EVENT_NEXT_SONG; 
							   else if (signals & SIGBREAKF_CTRL_E) return EVENT_SWITCH_TO_SEEK;
							   else if (signals & SIGBREAKF_CTRL_C) return EVENT_BREAK;
							   break;
			
			case MODE_SEEK : if (signals & SIGBREAKF_CTRL_D) return EVENT_DECREMENT_SEEK;
							 else if (signals & SIGBREAKF_CTRL_F) return EVENT_INCREMENT_SEEK; 
							 else if (signals & SIGBREAKF_CTRL_E) return SeekCtrlE();
							 else if (signals & SIGBREAKF_CTRL_C) return SeekCtrlC();
							 break;
							 
			case MODE_VOLUME : if (signals & SIGBREAKF_CTRL_D) return EVENT_DECREMENT_VOLUME;
							   else if (signals & SIGBREAKF_CTRL_F) return EVENT_INCREMENT_VOLUME; 
							   else if (signals & SIGBREAKF_CTRL_E) return VolumeCtrlE();
							   else if (signals & SIGBREAKF_CTRL_C) return VolumeCtrlC();
							   break;
		}
	}
}

UBYTE GetAndHandleNotIntEvents(UBYTE clr)
{
	UBYTE e = GetEvent(clr);
	HandleEventVarsAndPrints(e);
	return e;
}

UBYTE isNotIntEvent(UBYTE e)
{
	if (e==0) return 1;
	else if 
	   (
		(EVENT_NOTINT_START <= e)
		&&
		(e <= EVENT_NOTINT_END)
	   )
	   return 1;
	else return 0;
}

UBYTE isIntEvent(UBYTE e)
{
	if (e==0) return 0;
	else if 
	   (
		(EVENT_INT_START <= e)
		&&
		(e <= EVENT_INT_END)
	   )
	   return 1;
	else return 0;
}
