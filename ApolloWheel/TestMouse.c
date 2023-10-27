
#include <stdio.h>
#include <exec/types.h>

#define MOUSECOUNT *((UBYTE*)0xDFF212+1)
#define MIDDLEBUTTON (!(*((UWORD*)0xDFF016) & (1<<8)))
#define LEFTBUTTON (!(*(UBYTE*)0xBFE001 & (1<<6)))
#define RIGHTBUTTON (!(*(UWORD*)0xDFF016 & (1<<10)))

void WaitForMiddleButtonReleased(void) {
	while (MIDDLEBUTTON) ;
}

int main(void) {

	UBYTE old_wy, act_wy, cont;
	BYTE delta_wy;
	UBYTE mode;
	
	old_wy=act_wy=MOUSECOUNT;
	cont=1;
	mode=0;
	
	printf("Scroll or click mouse wheel - to interrupt, press left mouse button.\n");
	
	while (cont) {
		act_wy=MOUSECOUNT;
		if (act_wy != old_wy) 
		{
			delta_wy = act_wy-old_wy;
			if (delta_wy>127) delta_wy-=255;
			else if (delta_wy<-127) delta_wy+=255;
			
			printf("Weelcount: %d delta: %d\n", MOUSECOUNT, delta_wy);
			old_wy=act_wy;
		}
		//	printf("BFE001: %p (%d)\n", (void*)*(UBYTE*)0xBFE001, (!(*(UBYTE*)0xBFE001 & (1<<6))));
		//	printf("BFE001: %p (%d)\n", (void*)*(UWORD*)0xDFF016, RIGHTBUTTON);

		if (MIDDLEBUTTON) 
		{
			printf("MIDDLE: %d\n", MIDDLEBUTTON);
			printf("Wait ...\n");
			WaitForMiddleButtonReleased();
			printf("... ok\n");
			mode=((mode+1) & 1);
			printf("Mode: %d\n", mode);
		}
	
		if ((LEFTBUTTON) && (RIGHTBUTTON)) printf("SWITCH !!!!!!!!!!!!!!!!!!!!!!\n");
	
		if (LEFTBUTTON) cont=0;
	
	}
}

