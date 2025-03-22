
#ifndef MP3_H
#define MP3_H

#include "defines.h"

#include <libraries/mpega.h>
#include <clib/mpega_protos.h>
#include <pragmas/mpega_pragmas.h>

#include <dos.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include <proto/dos.h>
#include <exec/exec.h>
#include <clib/exec_protos.h>
#include <pragmas/exec_pragmas.h>
#include <exec/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <proto/timer.h>
#include <devices/timer.h>

UBYTE PrepareNewSeekDestination(LONG);
/*static ULONG __saveds __asm def_baccess( register __a0 struct Hook  *,
                                         register __a2 APTR,
                                         register __a1 MPEGA_ACCESS *);
*/										 
UBYTE PlayMP3(char*);

//extern static struct Hook def_bsaccess_hook;

extern WORD *pcmLR;
extern WORD *pcm_buffer;
extern WORD *pcm_end;

extern MPEGA_STREAM *mps;

extern BYTE   *mpega_buffer;
extern ULONG  mpega_buffer_offset;
extern ULONG  mpega_buffer_size;

extern UBYTE opt_verbose;

extern struct Library *DOSBase;

extern ULONG ms;
extern FILE* in_file;
extern char* in_filename;
extern MPEGA_CTRL mpa_ctrl;
extern UBYTE dirplay;
extern clock_t cl_vs;
extern ULONG act_seek_ms;
extern ULONG ms_bs;
extern ULONG E_Freq;
extern UBYTE loop_event;
extern clock_t cl_ve;
extern clock_t cl_volume_change_delta;
extern struct EClockVal ecl_bs, ecl_ab;
extern ULONG ms_be;
extern UWORD ts;
extern ULONG delta_ms;
extern ULONG next_ms;
extern ULONG sig_check_counter;
extern ULONG tstart;
extern ULONG tend;
extern UBYTE jump_activated;

extern struct Library *MPEGABase;
extern char mpega_name[];
extern char *mpega_name_ptr;
extern LONG pcm_count, total_pcm;
extern int frame;

#endif /* MP3_H */