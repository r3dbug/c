
#ifndef DEFINES_H
#define DEFINES_H

/* constants */
#define MPEGA_BUFFER_SIZE       (256*1024)                      /* in bytes*/
#define MAX_TITLES              100                             /* -t option */
#define MAX_FILE_LENGTH         300
#define FILENAMES_BUFFER_SIZE   (MAX_TITLES*MAX_FILE_LENGTH)
#define PCM_BUFFER_SIZE         3000000                         /* 16 secs, 16bit, 44.1 khz, stereo = 2'822'400 bytes */
#define FIX_TOTAL_PCM_LENGTH    706176                          /* best size: 2'824'704 */

/* stream types */
#define STREAM_TYPE_MPEGA		1
#define STREAM_TYPE_WAV			2
#define STREAM_TYPE_AIFF		3
#define STREAM_TYPE_FLAC		4

/* events */
#define EVENT_NOTHING			0

/* not interrupting / NOTINT */
#define EVENT_STREAM_DONE		1
#define EVENT_SWITCH_TO_SEEK	2
#define	EVENT_SWITCH_TO_VOLUME	3
#define EVENT_SWITCH_TO_NORMAL	4
#define EVENT_INCREMENT_SEEK	5
#define EVENT_DECREMENT_SEEK	6
#define EVENT_INCREMENT_VOLUME	7
#define EVENT_DECREMENT_VOLUME	8
#define EVENT_SEEK_CANCEL		9
#define EVENT_VOLUME_CANCEL		10

/* interrupting / INT */
#define EVENT_BREAK				11
#define EVENT_NEXT_SONG			12
#define EVENT_PREVIOUS_SONG		13
#define EVENT_EXECUTE_SEEK		14
#define EVENT_EXECUTE_VOLUME	15

/* start - end */
#define EVENT_INT_START			11
#define EVENT_INT_END			15
#define EVENT_NOTINT_START		1
#define EVENT_NOTINT_END		10
		
/* player modes */
#define MODE_NORMAL				0
#define MODE_SEEK				1
#define MODE_VOLUME				2

/* ARNE modes */

#define ARNE_STEREO				1<<2
#define ARNE_MONO				0<<2
#define ARNE_ONE_SHOT			1<<1
#define ARNE_LOOP				0<<1
#define ARNE_8BIT				1<<0
#define ARNE_16BIT				0<<0

#endif /* DEFINES_H */