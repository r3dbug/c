
#ifndef EVENTS_H
#define EVENTS_H

#include <clib/exec_protos.h>
#include <pragmas/exec_pragmas.h>
#include <dos.h>
#include <dos/dos.h>
#include <stdio.h>
#include <exec/exec.h>
#include <clib/exec_protos.h>
#include <pragmas/exec_pragmas.h>
#include <exec/types.h>
#include <time.h>
#include <ctype.h>
#include <proto/timer.h>
#include <devices/timer.h>

#include "defines.h"
#include "Spartakus.h"

extern UBYTE ctrl_mode;						
extern LONG  ctrl_seek;
extern LONG  ctrl_volume;
extern UBYTE ctrl_seek_modified;
extern UBYTE ctrl_volume_modified;

extern UBYTE opt_verbose;
extern UBYTE opt_loop;
extern UBYTE opt_reverse;
extern ULONG opt_seek_ms;
extern UWORD opt_max_titles;
extern UBYTE opt_quality;
extern UWORD opt_volume;
extern UBYTE opt_ARNE_main;
extern UBYTE opt_ARNE_secondary;
extern LONG opt_jump_bkw_ms;
extern LONG opt_jump_fwd_ms;
extern WORD opt_ARNE_mode;
extern UWORD opt_sample_rate;

extern clock_t cl_bs;
extern ULONG ms_ab;

extern ULONG ms_song_duration;
extern int next_title;

UBYTE SeekCtrlE(void);
UBYTE SeekCtrlC(void);
UBYTE VolumeCtrlE(void);
UBYTE VolumeCtrlC(void);
void UpdateSeek(LONG s);
void UpdateVolume(LONG v);
void EnterSeek(void);
void EnterVolume(void);
void EnterNormal(void);
void UpdateNormal(void);
void HandleEventVarsAndPrints(UBYTE);
UBYTE GetEvent(UBYTE);
UBYTE GetAndHandleNotIntEvents(UBYTE);
UBYTE isNotIntEvent(UBYTE);
UBYTE isIntEvent(UBYTE e);

#endif /* EVENTS_H */