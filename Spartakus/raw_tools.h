
/* functions and data structures to handle wav files in a similar
   way as MPEGA
 */

#ifndef RAW_TOOLS_H
#define RAW_TOOLS_H

/* data */

/* WAV */
struct WAVHeader {
    ULONG   FileTypeBlocID;             // RIFF: 0x52494646
    ULONG   FileSize;                       
    ULONG   FileFormatID;               // WAVE: 0x57415645
    ULONG   FormatBlocID;
    ULONG   BlocSize;
    UWORD   AudioFormat;                // 1: PCM integer 3: IEEE 754 float
    UWORD   NbrChannels;
    ULONG   Frequency;
    ULONG   BytePerSec;
    UWORD   BytePerBloc;
    UWORD   BitsPerSamle;
    ULONG   DataBlocID;
    ULONG   DataSize;
    /* end of header */
    UWORD   SampleData; 
};

/* AIFF */
struct CommonChunk
{
    ULONG   ckIDM;              // "COMM"
    ULONG   ckSize;
    UWORD   NumChannels;
    UWORD   empty; //?
    ULONG   NumSampleFrames;
    UWORD   SampleSize;
    UWORD   SampleRate;         // 0xAC44 (44.1 khz)
};

struct SoundDataChunk
{
    ULONG   ckID;               // "SSND"
    ULONG   ckSize;
    ULONG   Offset;
    ULONG   BlockSize;
    UBYTE*  SampleData;
};

/* functions */

UBYTE PlayWAV(char*, UBYTE);


#endif /* RAW_TOOLS_H */