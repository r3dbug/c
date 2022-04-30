
// simple SAGA example in C
// draws a gradient on 1280x720 ARGB screen
// switches to 640x360 screen after that and does some scrolling 
// opens an intuition window to "intercept" mouse events
// use mouseclick to step through the different parts of the program

// includes
#include <stdio.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <stdlib.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <intuition/screens.h>
#include <ctype.h>

// defines
#define SAGA_ERROR NULL

extern struct Library* DOSBase;
extern struct Library* IntuitionBase;

// buffer type
typedef struct {
	void* address;
	ULONG size;
} RAWBuffer;

RAWBuffer ThisBuffer;

// buffers
USHORT *saga_buffer;
USHORT free_saga_buffer;
unsigned long saga_mem;
USHORT *saga_unaligned;

// SAGA modes
int saga_modes[] = { 320, 200, 0x01,
                     320, 240, 0x02,
                     320, 256, 0x03,
                     640, 400, 0x04,
                     640, 480, 0x05,
                     640, 512, 0x06,
                     960, 540, 0x07,
                     480, 570, 0x08,
                     304, 224, 0x09,
                    1280, 720, 0x0a,
                     640, 360, 0x0b,
                     800, 600, 0x0c,
                    1024, 768, 0x0d,
                     720, 576, 0x0e,
                     848, 480, 0x0f,
                     640, 200, 0x10
                    };

// color depth
int color_depth[] = {  8, 0x01,   // in bits
                      16, 0x02,
                      15, 0x03,
                      24, 0x04,
                      32, 0x05
                    };

// resolution
int saga_resx, 
    saga_resy,
    saga_bits,
    saga_bytes;              

// old values
int old_mode,
    old_buffer;
    
// saga addresses
ULONG *fbr=(ULONG*)0xdfe1ec;
UWORD *mr=(UWORD*)0xdfe1f4;
ULONG *fbw=(ULONG*)0xdff1ec;
UWORD *mw=(UWORD*)0xdff1f4;

UWORD *modulo=(UWORD*)0xdff1e6;

UWORD *ciaapra=(UWORD*)0xbfe001;

// audio
ULONG *audio_buffer=(ULONG*)0xdff400;
ULONG *audio_length=(ULONG*)0xdff404;
UWORD *audio_volume=(UWORD*)0xdff408;
UWORD *audio_rate=(UWORD*)0xdff40c;
UWORD *audio_mode=(UWORD*)0xdff40a;
UWORD *audio_dma=(UWORD*)0xdff096;

// hardware
void WaitForMouseClick(void) {
	UWORD old_value=(*ciaapra);
	while (old_value==(*ciaapra));
}

void wfmc(void) {
	WaitForMouseClick();
}

// intuition window
// this window is only needed to "intercept" mouse events
// (that otherwhise would go to some other intuition task running in the background)

struct Window* myWindow;

struct TagItem wintags[] = {
//	WA_Left, 20, WA_Top, 20,
//	WA_Width, 200, WA_Height, 150,
	WA_IDCMP, IDCMP_CLOSEWINDOW,
	WA_Flags, /* WFLG_SIZEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | */ WFLG_ACTIVATE,
	WA_Title, "My Window",
	WA_PubScreenName, "Workbench",
	TAG_DONE
};

// lookup functions
// mode
int LookupMode(int resx, int resy) {
    int m, i, o, n=(sizeof(saga_modes)/sizeof(int))/3;
    m=SAGA_ERROR;
    for (i=0;i<n;i++) {
        o=i*3;
        if ((saga_modes[o]==resx) && (saga_modes[o+1]==resy)) m=saga_modes[o+2];
    }
    return m;
}

// format
int LookupPixelformat(int colors) {
    int f, i, o, n=(sizeof(color_depth)/sizeof(int))/2;
    f=SAGA_ERROR;
    for (i=0;i<n;i++) {
        o=i*2;
        if (color_depth[o]==colors) f=color_depth[o+1];
    }
    return f;
}

// draw functions
void PutPixelARGB32(int x, int y, ULONG color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((ULONG*)saga_buffer)+y*saga_resx+x)=color;
    }    
}

void PutPixelRGB24(int x, int y, ULONG color) {
    void* p;
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        p=saga_buffer+y*saga_resx*3+x*3;
        *((UBYTE*)p)=(UBYTE)((color & 0xff0000)>>16);
        *((UWORD*)(((UBYTE*)p)+1))=(UWORD)(color & 0xffff);
    }    
}

void PutPixelRGB16(int x, int y, UWORD color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((UWORD*)saga_buffer)+y*saga_resx+x)=color;
    }    
}

void PutPixelRGB15(int x, int y, UWORD color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((UWORD*)saga_buffer)+y*saga_resx+x)=color;
    }    
}

void PutPixelCLUT8(int x, int y, char color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        saga_buffer[y*saga_resx+x]=color;
    }
}

void PutPixel(int x, int y, ULONG color) {
    switch (saga_bits) {
        case  8 : PutPixelCLUT8(x,y,(char)color); break;
        case 15 : PutPixelRGB15(x,y,(UWORD)color); break;
        case 16 : PutPixelRGB15(x,y,(UWORD)color); break;
        case 24 : PutPixelRGB24(x,y,color); break;
        case 32 : PutPixelARGB32(x,y,color); break;     
    }
} 

// drawline
void PlotLinePixel(int x1, int y1, int x2, int y2, int dx, int dy, int decide, ULONG color) {
    //pk is initial decision making parameter
    //Note:x1&y1,x2&y2, dx&dy values are interchanged
    //and passed in plotPixel function so
    //it can handle both cases when m>1 & m<1
    int i;
	int pk = 2 * dy - dx;
    for (i = 0; i <= dx; i++) {
        //checking either to decrement or increment the value
        //if we have to plot from (0,100) to (100,0)
        x1 < x2 ? x1++ : x1--;
        if (pk < 0) {
            //decision value will decide to plot
            //either  x1 or y1 in x's position
            if (decide == 0) {
               // putpixel(x1, y1, RED);
                PutPixel(x1,y1, color);
				pk = pk + 2 * dy;
            } else {
                //(y1,x1) is passed in xt
               // putpixel(y1, x1, YELLOW);
                PutPixel(y1,x1, color);
				pk = pk + 2 * dy;
            }
        } else {
            y1 < y2 ? y1++ : y1--;
            if (decide == 0) {
               PutPixel(x1, y1, color);
            } else {
               PutPixel(y1, x1, color);
            }
            pk = pk + 2 * dy - 2 * dx;
        }
    }
}

void DrawLine2(int x1, int y1, int x2, int y2, ULONG color) {
	int dx = abs(x2-x1);
	int dy = abs(y2-y1);
	
	if (dx>dy) {
		PlotLinePixel(x1,y1,x2,y2,dx,dy,0,color);
	} else {
		PlotLinePixel(y1,x1,y2,x2,dy,dx,1,color);
	}
}

// screen functions
// open saga
USHORT*  OpenSAGAScreen(int resx, int resy, int colors, USHORT* buffer) {
    int mode, format;
    
    mode=LookupMode(resx,resy);
    format=LookupPixelformat(colors);
    
    // define bits and bytes
    saga_bits=colors;
    saga_bytes=(int)((saga_bits+1)/8);
    
    // set mode
    if ((mode!=SAGA_ERROR) && (format!=SAGA_ERROR)) {
        saga_resx=resx;
        saga_resy=resy;
        if (buffer==NULL) {
			saga_mem=(resx*resy*saga_bytes)+64;
            saga_unaligned=AllocVec(saga_mem, MEMF_PUBLIC | MEMF_CLEAR);
            saga_buffer=(USHORT*)((((((ULONG)saga_unaligned))+4)>>2)<<2); //// & (~7));
			free_saga_buffer=1;
        } else {
            if (buffer!=saga_buffer) {
                // buffer==saga_buffer: SwitchSAGAResolution calls OpenSAGAScreen with saga_buffer
                // in all other cases set new buffer
                saga_buffer=buffer;
                free_saga_buffer=0; // don't free it at the end (user is responsible for memory management)
            }
        }
        if (saga_buffer) {
            printf("SetMode: resx=%d resy=%d colors=%d => mode=%d format=%d buffer=%d (unaligned=%d) mem=%d\n", resx, resy, colors, mode, format, saga_buffer, saga_unaligned, saga_mem);
            // save old values
            old_mode=*mr;
            old_buffer=*fbr;
            // set new mode
            *fbw=(ULONG)saga_buffer;
            *mw=(mode<<8)+format;
            *modulo=0;
            return saga_buffer;
        } else return SAGA_ERROR; // return allocation error as SAGA_ERROR
    } else return SAGA_ERROR;
}

// switch saga
USHORT* SwitchSAGAResolution(int resx, int resy, int colors, USHORT* buffer, int mod) {
    USHORT *b;
    int bkp_old_mode=old_mode, bkp_old_buffer=old_buffer;
    printf("Switch ...\n");
    if (buffer==NULL) {
        b=OpenSAGAScreen(resx,resy,colors,saga_buffer);
    } else {
        b=OpenSAGAScreen(resx,resy,colors,buffer);
    }
    *modulo=mod;
    old_mode=bkp_old_mode;		// always keep old_mode and old_buffer from original saga screen
    old_buffer=bkp_old_buffer;
    return b;
}


// close
void CloseSAGAScreen(void) {   
    // restore old_mode
    *mw=old_mode;
    *fbw=old_buffer;
    *modulo=0;
    // free mem
    if (free_saga_buffer) {
        printf("Free SAGA-buffer: saga_buffer=%ld saga_mem=%ld\n",saga_buffer, saga_mem);
        FreeVec(saga_unaligned);
	}
}

// set new buffer position
void SetNewSAGABufferPosition(int dx, int dy, int width) {
    ULONG offset, o2, o3; 
    offset=(dy*width+dx)*saga_bytes;
    o2=offset&(~7);
	o3=offset | 4;
	//printf("saga_bytes=%d width=%d dx=%d, dy=%d => offset=%d (o2=%d, o3=%d)\n",saga_bytes,width, dx,dy,offset,o2,o3);
    *fbw=((ULONG)(saga_buffer+o2));	// make sure new buffer address is 64 bit aligned
}

// pictures
RAWBuffer* LoadRAWBuffer(char* filename, void* preallocated) {
	FILE* fh;
	void* buffer;
	ULONG size, t;
	
	fh=fopen(filename,"r");
    if (fh==NULL) return NULL;
    
    // get file size
    fseek(fh,0,2);
    size=ftell(fh);
    if (size==0) return NULL;

	// allocate buffer
    if (!preallocated) {
		buffer = AllocMem(size * sizeof(UBYTE), MEMF_ANY);
    	if (buffer==NULL) return NULL;
    	
	} else {
		buffer=preallocated;
	}
	// read file into buffer & close
    rewind(fh);
    t=fread(buffer,1, size, fh);
    if (t!=size) printf("Error: file size: %d read: %d\n",size,t);
    fclose(fh);

	// return RAWBuffer
	ThisBuffer.address=buffer;
	ThisBuffer.size=size;
	printf("LoadRAWBuffer(): %p\n", ThisBuffer.address);
	return &ThisBuffer;
}

void FreeRAWBuffer(RAWBuffer *rb) {
	if (rb->address) FreeMem(rb->address, rb->size);
}

void CopyRAW24ToRAW32(char* source, char* destination, ULONG size) {
	int i, s=0, d=0;
	for (i=0;i<size;i++) {
		destination[d++]=0;						// alpha
		destination[d++]=source[s++];			// red
		destination[d++]=source[s++];			// green
		destination[d++]=source[s++]; 			// blue
	}
}

void CopyRAWBufferPtr(RAWBuffer* a, RAWBuffer* b) {
	b->address=a->address;
	b->size=a->size;
}

// text functions
void PrintChar(int px, int py, char pc, char* address) {
	ULONG *source;
	char c=toupper(pc)-'A';
	int offset;	// font alpha2.data: 12x24 ARGB (32bit)
	ULONG argb;			
	int x, y;
	

	if (pc==' ') return;
	if (pc=='.') c=26;

	offset=c*12*4*24;
	source=(ULONG*)(address+offset);
						
	for (y=0;y<24;y++) {
		for (x=0;x<12;x++) {
			//*dest++=*source++;				
			argb=(*source++); //+0x000000ff;
			//PutPixelARGB32(px+x,py+y,argb);
			if (c==0) {
				if (argb >= 0x00ffffff) {
					// correction for character 0 == 'A'
					PutPixelARGB32(px+x,py+y,argb);
				}
			} else if (argb != 0x00ffffff) {
				// white = transparent!
				PutPixelARGB32(px+x,py+y,argb);
			}
		}	
	}
}

void PrintString(int px, int py, char* string, char* address) {
	int x, i;
	i=0; x=px;
	while (string[i]!=0) {
		PrintChar(x,py, string[i], address);
		i+=1;
		x+=14;
	}
}

// play music
void PlayMusic(void* buffer, ULONG length, UWORD volume, UWORD rate, UWORD mode) {
	*audio_buffer=((ULONG)buffer)+64; // skip aiff header
	*audio_length=(length-64)/8;
	*audio_volume=volume;
	*audio_rate=rate;
	*audio_mode=mode;
	*audio_dma=0x8201;
	printf("PlayMusic(): buffer=%d length=%d volume=%d rate=%d mode=%d\n",buffer,length,volume,rate,mode);
}

void StopMusic(void) {
	*audio_buffer=NULL;
	*audio_length=0;
	*audio_volume=0;
	*audio_mode=0;
	*audio_dma=1; // turn dma channel 0 off
}

// main
int main(void) {
    int x,y,c,base;
	ULONG* dest, *source;
	RAWBuffer music, font;
	ULONG argb;
	
//    int r=0,g=0,b=0;
    
	//wfmc();
	
	// open intuition window to "intercept" mouse events
	myWindow=OpenWindowTagList(NULL, &wintags[0]);
	
	//wfmc();
	
    // now open a 1280x720x32 saga screen
	if (OpenSAGAScreen(1280,720,32,NULL)==SAGA_ERROR) {
        printf("Unable to open SAGA screen ...\n");
    } else {
	
		// draw some lines
		for (x=0;x<saga_resx;x+=3) {
			DrawLine2(640,360,x,saga_resy-1,0x00ff0000);	
			DrawLine2(640,360,x,0,0x0000ff00);
		}
		for (y=0;y<saga_resy;y+=3) {
			DrawLine2(640,360,0,y,0x00ffff00);	
			DrawLine2(640,360,saga_resx-1,y, 0x000000ff);
		}
	
		DrawLine2(0,360,640,360,0x00ffffff);
		DrawLine2(1279,360,640,360,0x00ffffff);
		DrawLine2(640,0,640,360,0x00ffffff);
		DrawLine2(640,719,640,360,0x00ffffff);
		
		wfmc();
		
		// load raw background
		LoadRAWBuffer("batman.raw", NULL);
		CopyRAW24ToRAW32(ThisBuffer.address, (void*)saga_buffer, 1280*720);
		FreeRAWBuffer(&ThisBuffer);
		wfmc();	

		// Load and play music
		CopyRAWBufferPtr(LoadRAWBuffer("eagle_44khz.aiff", NULL), &music);
		//LoadRAWBuffer("dh2:downloads/wav/01_coitus_int_a_birds.aiff", NULL);
		PlayMusic(ThisBuffer.address, ThisBuffer.size, 0xffff, 80, 7);
	
		// print some text
		CopyRAWBufferPtr(LoadRAWBuffer("alpha2.data", NULL), &font); // alpha2.data from Arne Demo 002
		PrintString(430,190, "Ok... lets do some text now ...", font.address);
		Delay(4);
		wfmc();
		
		// switch to screen with half the resolution
		// => this way we can use original buffer for scrolling
		SwitchSAGAResolution(640,360,32,saga_buffer,640*saga_bytes);
        
		// scrolling on 640x360x32 screen (using screen buffer from 1280x720x32)
		y=0;
        for (x=30;x<=160;x++) {
            y=x/2;
            Delay(1);
            SetNewSAGABufferPosition(x,y, 640);
			//wfmc();
        }
        wfmc();
        
        // switch back to first saga screen
        SwitchSAGAResolution(1280,720,32,saga_buffer,0);
        
        // close saga screen
		CloseSAGAScreen();
    } 
	
	// close intuition window
	if (myWindow) CloseWindow(myWindow);

	StopMusic(); // just to be sure that it stops
	FreeRAWBuffer(&music);

	printf("ThisBuffer.address=%p ThisBuffer.size=%d font.address=%p font.size=%d\n", ThisBuffer.address, ThisBuffer.size, font.address, font.size);
	FreeRAWBuffer(&font);
}