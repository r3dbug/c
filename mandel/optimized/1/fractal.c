
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


void PutPixelARGB32(int x, int y, ULONG color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((ULONG*)saga_aligned)+y*saga_resx+x)=color;
        //saga_aligned[(y*1280*4)+(x*4)]=color;
    }    
}

void PutPixelRGB24(int x, int y, ULONG color) {
    void* p;
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        p=saga_aligned+y*saga_resx*3+x*3;
        *((UBYTE*)p)=(UBYTE)((color & 0xff0000)>>16);
        *((UWORD*)(((UBYTE*)p)+1))=(UWORD)(color & 0xffff);
    }    
}

void PutPixelRGB16(int x, int y, UWORD color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((UWORD*)saga_aligned)+y*saga_resx+x)=color;
    }    
}

void PutPixelRGB15(int x, int y, UWORD color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        *(((UWORD*)saga_aligned)+y*saga_resx+x)=color;
    }    
}

void PutPixelCLUT8(int x, int y, char color) {
    if ((x>=0) && (x<saga_resx) && (y>=0) && (y<saga_resy)) {
        saga_aligned[y*saga_resx+x]=color;
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
        saga_mem=(resx*resy*saga_bytes)+8;
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

ULONG gettime(void) {
    ULONG start, stop;
    start=getstart();
    stop=getstop();
    return (stop>start) ? (stop-start) : (0xffffffff-(start-stop));
}

int main(void) {
    // variables
    int x, y;
    double fx, fy;
    double minx, maxx, miny, maxy;
    double cx, cy, zn1x, zn1y, temp;
    char *screen;
    ULONG it;
    int color;
    ULONG clocks;
    double seconds;
    ULONG frequency=12*7.09*1000000; // 12x core
    ULONG maxit=256;
    
    screen=OpenSAGAScreen(1280,720,8);
    
    for (color=0;color<256;color++) {
        setcolor(color,rand()%256, rand()%256, rand()%256);
    }
    setcolor(0,0,0,0);
    
    minx=-2.25;
    maxx=1.25;
    miny=-1.25;
    maxy=1.25;
    
    fx=(double)(maxx-minx)/(double)saga_resx;
    fy=(double)(maxy-miny)/(double)saga_resy;
    
    setstart();
    for (y=0;y<saga_resy;y++) {
        for (x=0;x<saga_resx;x++) {
            cx=x*fx+minx;
            cy=y*fy+miny;
            zn1x=0;
            zn1y=0;
            it=0;
            do {
               temp=zn1x*zn1x-zn1y*zn1y+cx;
               zn1y=2*zn1x*zn1y+cy;
               zn1x=temp;
               it++;
            } while (((zn1x*zn1x+zn1y*zn1y)<4) && (it<maxit));
            PutPixel(x,y,it%256);
        }
    }
    setstop();
    waitmouse();
    restoremode();
   
    clocks=gettime();
    seconds=(double)clocks/(double)frequency;
    printf("Time: clocks=%u seconds=%lf\n",clocks,seconds);
}