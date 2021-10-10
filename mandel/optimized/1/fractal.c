
#include <stdio.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <stdlib.h>

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
    saga_mem,
    saga_bits,
    saga_bytes;              
  
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


void CloseSAGAScreen() {   
    restoremode();
    FreeMem(saga_buffer,saga_mem);
}

ULONG gettime(void) {
    ULONG start, stop;
    start=getstart();
    stop=getstop();
    return (stop>start) ? (stop-start) : (0xffffffff-(start-stop));
}

int main(void) {
    // variables
    int x, y;
    double hfx, hfy, vfx, vfy;
    double sx[4], sy[4], lx, ly;
    double cx, cy, znx, zny, zn1x, zn1y;
    char *screen;
    ULONG it;
    int invit;
    int color;
    ULONG clocks;
    double seconds;
    ULONG frequency=12*7.09*1000000; // 12x core
    ULONG maxit=256;
    unsigned long screen_offset=0;
    
    saga_resx=1280;
    saga_resy=720;
    screen=OpenSAGAScreen(1280,720,8);
    
    for (color=1;color<256;color++) {
        setcolor(color,rand()%256, rand()%256, rand()%256);
    }
    setcolor(0,0,0,0);
   
    /*
    sx[0]=-2.25; sy[0]=-1.6;
    sx[1]=1.25; sy[1]=-1.35;
    sx[2]=1; sy[2]=1.5; // not used
    sx[3]=-2.5; sy[3]=1.25; 
    */
    sx[0]=-2.25; sy[0]=-1.25;
    sx[1]=1.25; sy[1]=-1.25;
    sx[2]=1; sy[2]=1.25; // not used
    sx[3]=-2.25; sy[3]=1.25; 
    
    hfx=(double)(sx[1]-sx[0])/(double)saga_resx;
    hfy=(double)(sy[1]-sy[0])/(double)saga_resy;
    vfx=(double)(sx[3]-sx[0])/(double)saga_resx;
    vfy=(double)(sy[3]-sy[0])/(double)saga_resy;
    
    setstart();
    lx=sx[0];
    ly=sy[0];
    cy=ly;
    for (y=0;y<saga_resy;y++) {
        cx=lx; 
        for (x=0;x<saga_resx;x++) {    
            
            zn1x=znx=0;
            zn1y=zny=0;
            it=0;
            //it=(UWORD)calcpoint(maxit,cx, cy);  // doesn't work
            while ((zn1x+zn1y <= 4) && (it < maxit)) {
              zny=2*znx*zny+cy;
              znx=zn1x-zn1y+cx;
              zn1x=znx*znx;
              zn1y=zny*zny;    
              it++;
            }     
            saga_aligned[screen_offset++]=(char)it%256;
            cx+=hfx;
            cy+=hfy;
        }
        lx+=vfx;
        ly+=vfy;
        cy=ly;
    }
    setstop();
    waitmouse();
    CloseSAGAScreen();
   
    clocks=gettime();
    seconds=(double)clocks/(double)frequency;
    printf("Time: clocks=%u seconds=%lf\n",clocks,seconds);
}