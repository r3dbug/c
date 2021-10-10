
/* Mandelbrot calculation with boundary trace 
 *
 * Original algorithm: (c) Joel Yliluoma (2010)
 *
 * See:
 * https://bisqwit.iki.fi/jutut/kuvat/programming_examples/mandelbrotbtrace.pdf
 * https://www.youtube.com/watch?v=rVQMaiz0ydk
 *
 * Adapted for Vampire Standalone (V4SA) using SASC and VASM
 *
 */


#include <stdio.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <stdlib.h>
// #include <math.h> // only needed for cosine setcolor function

char *saga_buffer, 
    *saga_aligned;
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
int saga_number = 16;
int color_depth[] = {  8, 0x01,   // in bits
                      16, 0x02,
                      15, 0x03,
                      24, 0x04,
                      32, 0x05
                    };
int color_number = 5; 
int saga_resx, 
    saga_resy,
    saga_bits,
    saga_bytes;              
unsigned long saga_mem;

double cre,cim,diam,minr, mini, maxr, maxi, stepr, stepi;
unsigned Width;
unsigned Height;
unsigned MaxIter=256;

int             *Data;
unsigned char   *Done;
unsigned long   *Queue;

unsigned long DataSize;
unsigned long DoneSize;
unsigned long QueueSize;

unsigned QueueHead=0, QueueTail=0;

enum { Loaded=1, Queued=2 };

char* __asm pokemode(register __d0 unsigned int mode, 
                    register __d1 unsigned int format, 
                    register __a0 char* buffer);
void __asm restoremode(void);
void __asm fillscreen(register __d0 int resx,
                      register __d1 int resy,
                      register __d2 char color);
void __asm waitmouse(void);
void __asm setcolor( register __d0 int color,
                     register __d1 int red,
                     register __d2 int green,
                     register __d3 int blue
             );
void __asm setstart(void);
void __asm setstop(void);
ULONG __asm getstart(void);
ULONG __asm getstop(void);
UWORD __asm calcpoint(register __d0 UWORD it,
                      register __fp0 double cx,
                      register __fp1 double cy);

__inline void PutPixelARGB32(int x, int y, ULONG color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((ULONG*)saga_aligned)+y*saga_resx+x)=color;
        //saga_aligned[(y*1280*4)+(x*4)]=color;
    }    
}

__inline void PutPixelRGB24(int x, int y, ULONG color) {
    void* p;
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        p=saga_aligned+y*saga_resx*3+x*3;
        *((UBYTE*)p)=(UBYTE)((color & 0xff0000)>>16);
        *((UWORD*)(((UBYTE*)p)+1))=(UWORD)(color & 0xffff);
    }    
}

__inline void PutPixelRGB16(int x, int y, UWORD color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((UWORD*)saga_aligned)+y*saga_resx+x)=color;
    }    
}

__inline void PutPixelRGB15(int x, int y, UWORD color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((UWORD*)saga_aligned)+y*saga_resx+x)=color;
    }    
}

__inline void PutPixelCLUT8(int x, int y, char color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        saga_aligned[y*saga_resx+x]=color;
    }
}

__inline void PutPixel(int x, int y, ULONG color) {
    switch (saga_bits) {
        case  8 : PutPixelCLUT8(x,y,(char)color); break;
        case 15 : PutPixelRGB15(x,y,(UWORD)color); break;
        case 16 : PutPixelRGB15(x,y,(UWORD)color); break;
        case 24 : PutPixelRGB24(x,y,color); break;
        case 32 : PutPixelARGB32(x,y,color); break;     
    }
} 

char*  OpenSAGAScreen(int resx, int resy, int colors) {
    int mode, format;
    char *buffer, *aligned;
    int n;
    
    for (n=0;n<saga_number;n++) {
        if ((saga_modes[n*3]==resx) && (saga_modes[n*3+1]==resy)) mode=saga_modes[n*3+2];
    }
    for (n=0;n<color_number;n++) {
        if (color_depth[n*2]==colors) format=color_depth[n*2+1];
    }
    saga_bits=colors;
    saga_bytes=(int)((saga_bits+1)/8);
    if ((mode!=0) && (format!=0)) {
        saga_resx=resx;
        saga_resy=resy;
        saga_mem=(resx*resy*saga_bytes)+32;
        buffer=AllocMem(saga_mem, MEMF_PUBLIC | MEMF_CLEAR);
        saga_buffer=buffer;
        if (buffer) {
            printf("pokemode: resx=%d resy=%d colors=%d => mode=%d format=%d buffer=%d mem=%d\n", resx, resy, colors, mode, format, buffer, saga_mem);
            aligned=pokemode(mode,format,buffer);    
            saga_aligned=aligned;
            return aligned;
        } else return 0;
    } else return 0;
}

void CloseSAGAScreen(void) {   
    restoremode();
    printf("Free SAGA-buffer: saga_buffer=%ld saga_mem=%ld\n",saga_buffer, saga_mem);
    FreeMem(saga_buffer,saga_mem);
}

ULONG gettime(void) {
    ULONG start, stop;
    start=getstart();
    stop=getstop();
    return (stop>start) ? (stop-start) : (0xffffffff-(start-stop));
}

// ************* original boundary trace functions

int Iterate(double x,double y) {
    int iter;
    double r2, ri, i2;
    double r=x, i=y;        /* z = {r,i} */
    for(iter=0; iter<MaxIter; ++iter) {
        r2 = r*r;
        i2 = i*i;
        if(r2+i2 >= 4.0) break;
        // using this faster formula gives about 6% speed increase
        // 4.46 secs instead of 4.77 secs (1280x720, 256 colors, 256 iterations)
        i=2*r*i+y;
        r=r2-i2+x;
        //ri = r*i;
        //i = ri+ri + y;
        //r = r2 - i2 + x;
    }
    return iter;
}

void AddQueue(unsigned p) {
    
    if(Done[p] & Queued) return;
    Done[p] |= Queued;
    Queue[QueueHead++] = p;
    if(QueueHead == QueueSize) QueueHead = 0;
}

int Load(unsigned p) {
    unsigned x,y;
    int result;
    if(Done[p] & Loaded) return Data[p];
    x = p % Width;
    y = p / Width;
    result = Iterate(minr + x*stepr, mini + y*stepi);
    // direct buffer writing gives speed increase from 4.46 to 4.38
    // (= 2%)
    saga_aligned[y*saga_resx+x]=result;
    // PutPixel(x,y, result);
    Done[p] |= Loaded;
    return Data[p] = result;
}

void Scan(unsigned p) {
    unsigned x = p % Width, y = p / Width;
    int center = Load(p);
    int ll = x >= 1, rr = x < Width-1;
    int uu = y >= 1, dd = y < Height-1;
    /* booleans */
    int l=ll && Load(p-1) != center;
    int r=rr && Load(p+1) != center;
    int u=uu && Load(p-Width) != center;
    int d=dd && Load(p+Width) != center;
    /* process the queue (which is actually a ring buffer) */
    if (l) AddQueue(p-1);
    if (r) AddQueue(p+1);
    if (u) AddQueue(p-Width);
    if (d) AddQueue(p+Width);
    /* the corner pixels (nw,ne,sw,se) are also neighbors */
    if((uu&&ll)&&(l||u)) AddQueue(p-Width-1);
    if((uu&&rr)&&(r||u)) AddQueue(p-Width+1);
    if((dd&&ll)&&(l||d)) AddQueue(p+Width-1);
    if((dd&&rr)&&(r||d)) AddQueue(p+Width+1);
}
    
// *************** end of original boundary trace functions

void AllocateAllMem(void) {
    DataSize=sizeof(int)*Width*Height;
    DoneSize=sizeof(unsigned char)*Width*Height;
    QueueSize=sizeof(unsigned long)*((Width*Height)*4); // + in original (must be *!)
    Data=AllocMem(DataSize, MEMF_PUBLIC | MEMF_CLEAR);
    Done=AllocMem(DoneSize, MEMF_PUBLIC | MEMF_CLEAR);
    Queue=AllocMem(QueueSize, MEMF_PUBLIC | MEMF_CLEAR);
    
}

void DeallocateAllMem(void) {
    printf("FreeMem: Data=%ld DataSize=%ld Done=%ld DoneSize=%ld Queue=%ld QueueSize=%ld\n", 
                     Data, DataSize, Done, DoneSize, Queue, QueueSize);
    FreeMem(Queue,QueueSize);
    FreeMem(Done,DoneSize);
    FreeMem(Data,DataSize);
}

int main(void) {
    int color;
    char* screen;
    unsigned x,y,p;
    unsigned flag;
    ULONG clocks;
    double seconds;
    ULONG frequency=12*7.09*1000000; // 12x core
   
    saga_resx=1280;
    saga_resy=720;
    screen=OpenSAGAScreen(saga_resx,saga_resy,8);
    Width=saga_resx;
    Height=saga_resy;
    
    /* 
    // original coordinates
    cre=-1.36022;
    cim=0.0653316; 
    diam=0.035;
    minr=cre-diam*0.5; 
    mini=cim-diam*0.5;
    maxr=cre+diam*0.5;
    maxi=cim+diam*0.5;
    */
    cre=-2.25;
    cim=-1.25;
    minr=-2.25;
    maxr=1.5;
    mini=-1.25;
    maxi=1.25;
    
    stepr = (maxr-minr) / Width;
    stepi = (maxi-mini) / Height;
    
    AllocateAllMem();
    
    setcolor(0,0,0,0);
    for (color=1;color<256;color++) {
        setcolor(color,rand()%256, rand()%256, rand()%256);
        /*
        // original color algorithm
        setcolor(color, 
            (int)(-31*cos(color*0.01227*1)),
            (int)(32-31*cos(color*0.01227*2)),
            (int)(32-31*cos(color*0.01227*5)));
        */
    }
    
    setstart();
    // original main part
    // begin by adding the screen edges into the queue 
    for(y=0; y<Height; ++y) {
        AddQueue(y*Width + 0);
        AddQueue(y*Width + (Width-1));
    }
    for(x=1; x<Width-1; ++x) {
        AddQueue(0*Width + x);
        AddQueue((Height-1)*Width + x);
    }
    // process the queue (which is actually a ring buffer) 
    flag=0;
    while(QueueTail != QueueHead) {
        if(QueueHead <= QueueTail || ++flag & 3) {
            p = Queue[QueueTail++];
            if(QueueTail == QueueSize) QueueTail=0;
        } else p = Queue[--QueueHead];
        Scan(p);
    }

    // lastly, fill uncalculated areas with neighbor color 
    for(p=0; p<Width*Height; ++p) {
        if(Done[p] & Loaded)
        if(!(Done[p+1] & Loaded)) {
            saga_aligned[p+1] = saga_aligned[p]; 
            Done[p+1] |= Loaded;
        }
    }
    setstop();
    
    waitmouse();
    CloseSAGAScreen();  
    DeallocateAllMem();
    
    clocks=gettime();
    seconds=(double)clocks/(double)frequency;
    printf("Time: clocks=%u seconds=%lf\n",clocks,seconds);
    
    return 0;
}