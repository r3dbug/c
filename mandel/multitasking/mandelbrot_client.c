
/* Classic Mandelbrot calculation implemented as:
 * - Superbitmap window with scrollbars 
 * - Client/server application with message ports
 *
 * Compile with SAS/C (developped under V6.59):
 *
 * smake all
 *
 *
 * Start from Shell with:
 *
 * execute mandelbrot_start
 *
 */

/* includes */
#include <proto/exec.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <exec/ports.h>
#include <exec/nodes.h>

#include <clib/exec_protos.h>
#include <clib/layers_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>
#include <proto/diskfont.h>
#include <graphics/text.h>
#include <graphics/gfx.h>
#include <stdlib.h>
#include <proto/layers.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#include "mandelbrot_common.h"

/* defines */
/* superbitmap width and height */
#define WIDTH_SUPER     (800)
#define HEIGHT_SUPER    (600)

/* gadgets */
#define UP_DOWN_GADGET    (0)
#define LEFT_RIGHT_GADGET (1)
#define NO_GADGET         (2)
#define ZOOM_GADGET		  (6)
#define MAXPROPVAL (0xFFFFL)
#define GADGETID(x) (((struct Gadget *)(msg->IAddress))->GadgetID)
#define LAYERXOFFSET(x) (x->RPort->Layer->Scroll_X)
#define LAYERYOFFSET(x) (x->RPort->Layer->Scroll_Y)

/* mandelbrot min and max start values */
#define MINX (-2)
#define MAXX (1)
#define MINY (-1)
#define MAXY (1)

/* message variables */
char PortName[]="MandelbrotServer";
struct MsgPort *myMsgPort;
struct MsgPort *myReplyPort;
struct MandelbrotMsg *ClientMsg;
struct MandelbrotMsg *ServerMsg;
struct Message *myReply;
UBYTE* ServerBitmapPtr;
/* zoom gadget */

UWORD buttonBorderData[] = {
	0,0,WIDTH_SUPER,0,WIDTH_SUPER,HEIGHT_SUPER,0,HEIGHT_SUPER,0,0,
};

struct Border buttonBorder = {
	-1,-1,1,0,JAM1,5,buttonBorderData,NULL,
};
		
/* mandelbrot variables */
double  minx, maxx, 
       miny, maxy, 
       ax, ay,
       deltax, deltay,
       fx, fy,
       dist, maxdist,
       cx, cy,
       zxn1, zyn1,
       tx, ty,
       temp1, temp2, temp3;

double zoom[4];
int    zoom_point;

int    it, maxit,
       stepx, stepy,
       noevents,
       eventcounter;

int    tmp, dX, dY;
unsigned int memory_total=0, memory_alloc=0;

/* update variables */
int    last_update_y,
       new_update_y;
double *pax, *pay;
UBYTE  allow_drawing;

            
/* custom functions */
UWORD RangeRand(unsigned long maxValue );
int randomNum(int n) { return (rand()%n); }

/* structs */
struct Library *GfxBase;
struct Library *IntuitionBase;
struct Library *LayersBase;

struct Window      *Win = NULL;          /* window pointer */
struct PropInfo     BotGadInfo = {0};
struct Image        BotGadImage = {0};
struct Gadget       BotGad = {0};
struct PropInfo     SideGadInfo = {0};
struct Image        SideGadImage = {0};
struct Gadget       SideGad = {0};
struct Screen 	   *myscreen;
struct Gadget       ZoomGadget={0};

/* menu definition */
struct IntuiText text1 = { 0, 1, JAM2, 4, 2, NULL, "MaxIt", NULL };
struct IntuiText text2 = { 0, 1, JAM2, 4, 2, NULL, "Update", NULL };
struct IntuiText text3 = { 0, 1, JAM2, 4, 2, NULL, "Zoom+", NULL };
struct IntuiText text4 = { 0, 1, JAM2, 4, 2, NULL, "Zoom-", NULL };
struct IntuiText text5 = { 0, 1, JAM2, 4, 2, NULL, "Quit", NULL };

struct MenuItem item5 = { NULL, 0, 48, 48, 12, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0, &text5, &text5, NULL, NULL, 0 };
struct MenuItem item4 = { &item5, 0, 36, 48, 12, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0, &text4, &text4, NULL, NULL, 0 };
struct MenuItem item3 = { &item4, 0, 24, 48, 12, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0, &text3, &text3, NULL, NULL, 0 };
struct MenuItem item2 = { &item3, 0, 12, 48, 12, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0, &text2, &text2, NULL, NULL, 0 };
struct MenuItem item1 = { &item2, 0, 0, 48, 12, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0, &text1, &text1, NULL, NULL, 0 };

struct Menu menu1 ={ NULL, 0, 0, 48, 12, MENUENABLED, "File", &item1, 0, 0, 0, 0 };

/* global variables */
UWORD menuNumber, menuNum, itemNum, subNum;
ULONG msgClass, line=2;
ULONG mypen, red, green, blue;
ULONG inner_width, inner_height;
struct RastPort *rp;
struct ColorMap *cm;

/* variables to print text in window */
UBYTE result[255];

struct IntuiText WinText = {1, 0, JAM1, 0, 0, NULL, &result[0], NULL};

/* drawing variables */
long x1, y1, x2, y2, color;
long posx=2;

/* control variables */
ULONG closewin=FALSE;

/* subtask control functions */

void BlockSubtaskDrawing(void) {
	allow_drawing=0;
}

void AllowServerDrawing(void) {
	allow_drawing=1;
}

/* mandelbrot functions */
void MandelbrotZoom(double dminx, double dmaxx, double dminy, double dmaxy) {
	printf("CLIENT: Last server message id=%d\n", ServerMsg->id);
	printf("CLIENT: Actual message id=%d\n", message_id);
	if (ServerMsg->id == message_id-1) {
		minx+=dminx;
		maxx+=dmaxx;
		miny+=dminy;
		maxy+=dmaxy;
		ClientMsg->type=REQ_CALC_START;
		ClientMsg->id=message_id;
		ClientMsg->x1=minx;
		ClientMsg->y1=miny;
		ClientMsg->x2=maxx;
		ClientMsg->y2=maxy;
		ClientMsg->resx=WIDTH_SUPER;
		ClientMsg->resy=HEIGHT_SUPER;
		ClientMsg->mn.mn_ReplyPort=myReplyPort;
		PutMsg(myMsgPort,(struct Message*)(void*)ClientMsg);
		WaitPort(myReplyPort);
		ServerMsg=(struct MandelbrotMsg*)(void*)GetMsg(myReplyPort);
      		if ((ServerMsg->type == RPL_CALC_START) && (ServerMsg->id==message_id)) {
			printf("CLIENT: Server has confirmed and started new calculation.\n");
			last_update_y=0;
			message_id++;		
		} else printf("CLIENT: Brother, you've got a problem ... :)\n");
	} else {
		printf("CLIENT: Menu selection can't be sent - server has to reply to last message first.\n");
	}
}

void MandelbrotZoomP1P2(double px1, double py1, double px2, double py2) {
	double fx, fy;
	double  deltax,deltay;
	printf("CLIENT: Last server message id=%d\n", ServerMsg->id);
	printf("CLIENT: Actual message id=%d\n", message_id);
	printf("CLIENT: MandelbrotZoomP1P2: (%lf,%lf)-(%lf,%lf)\n",px1,py1,px2,py2);
	if (ServerMsg->id == message_id-1) {
		deltax=maxx-minx;
		deltay=maxy-miny;
		fx=(maxx-minx)/WIDTH_SUPER;
		fy=(maxy-miny)/HEIGHT_SUPER;
		maxx=px2*fx+minx;
		minx=px1*fx+minx;
		maxy=py2*fy+miny;
		miny=py1*fy+miny;
		printf("CLIENT: deltax=%lf deltay=%lf fx=%lf fy=%lf\n",deltax,deltay,fx,fy);
		printf("CLIENT: zoom values: minx=%lf maxx=%lf miny=%lf maxy=%lf\n",minx,maxx,miny,maxy);
	
		ClientMsg->type=REQ_CALC_START;
		ClientMsg->id=message_id;
		ClientMsg->x1=minx;
		ClientMsg->y1=miny;
		ClientMsg->x2=maxx;
		ClientMsg->y2=maxy;
		ClientMsg->resx=WIDTH_SUPER;
		ClientMsg->resy=HEIGHT_SUPER;
		ClientMsg->mn.mn_ReplyPort=myReplyPort;
		PutMsg(myMsgPort,(struct Message*)(void*)ClientMsg);
		WaitPort(myReplyPort);
		ServerMsg=(struct MandelbrotMsg*)(void*)GetMsg(myReplyPort);
      		if ((ServerMsg->type == RPL_CALC_START) && (ServerMsg->id==message_id)) {
			printf("CLIENT: Server has confirmed and started new calculation.\n");
			last_update_y=0;
			message_id++;		
		} else printf("CLIENT: Brother, you've got a problem ... :)\n");
	} else {
		printf("CLIENT: Menu selection can't be sent - server has to reply to last message first.\n");
	}
}

VOID InitMandelbrotValues(void) {
	minx=MINX;
	maxx=MAXX;
	miny=MINY;
	maxy=MAXY;
}

/* gadgets */

/* vertical/horizontal scrollers */

VOID InitScrollerGadgets(struct Screen *myscreen) {
	/* initializes the two scroller gadgets. */
	BotGadInfo.Flags     = AUTOKNOB | FREEHORIZ | PROPNEWLOOK;
	BotGadInfo.HorizPot  = 0;
	BotGadInfo.VertPot   = 0;
	BotGadInfo.HorizBody = (unsigned short)-1; // explicit type cast to avoid warning
	BotGadInfo.VertBody  = (unsigned short)-1;

	BotGad.LeftEdge     = 3;
	BotGad.TopEdge      = -7;
	BotGad.Width        = -23;
	BotGad.Height       = 6;

	BotGad.Flags        = GFLG_RELBOTTOM | GFLG_RELWIDTH;
	BotGad.Activation   = GACT_RELVERIFY | GACT_IMMEDIATE | GACT_BOTTOMBORDER;
	BotGad.GadgetType   = GTYP_PROPGADGET | GTYP_GZZGADGET;
	BotGad.GadgetRender = (APTR)&(BotGadImage);
	BotGad.SpecialInfo  = (APTR)&(BotGadInfo);
	BotGad.GadgetID     = LEFT_RIGHT_GADGET;

	SideGadInfo.Flags     = AUTOKNOB | FREEVERT | PROPNEWLOOK;
	SideGadInfo.HorizPot  = 0;
	SideGadInfo.VertPot   = 0;
	SideGadInfo.HorizBody = (unsigned short)-1;
	SideGadInfo.VertBody  = (unsigned short)-1;

	SideGad.LeftEdge   = -14;
	SideGad.TopEdge    = myscreen->WBorTop + myscreen->Font->ta_YSize + 2;
	SideGad.Width      = 12;
	SideGad.Height     = -SideGad.TopEdge - 11;

	SideGad.Flags        = GFLG_RELRIGHT | GFLG_RELHEIGHT;
	SideGad.Activation   = GACT_RELVERIFY | GACT_IMMEDIATE | GACT_RIGHTBORDER;
	SideGad.GadgetType   = GTYP_PROPGADGET | GTYP_GZZGADGET;
	SideGad.GadgetRender = (APTR)&(SideGadImage);
	SideGad.SpecialInfo  = (APTR)&(SideGadInfo);
	SideGad.GadgetID     = UP_DOWN_GADGET;
	SideGad.NextGadget   = &(BotGad);
}

VOID InitZoomGadget(struct Screen* myscreen) {
	/* in order to be able to handle events for a zoom function we create a
	 * "dummy" gadget, i.e. an empty (invisible) gadget that will have the
	 * size of GZZWidth / GZZHeight, so that all messages can be identifiet
	 * via this gadget (id = ZOOM_GADGET). 
	 */
	ZoomGadget.NextGadget 		= &(SideGad); // connect zoom gadget to SideGad
	ZoomGadget.LeftEdge 		= 0;
	ZoomGadget.TopEdge			= 0;
	ZoomGadget.Width			= 79; // adapt later to GZZWidth
	ZoomGadget.Height			= 59; // adapt later to GZZHeight
	ZoomGadget.Flags			= GFLG_GADGHCOMP; 
	ZoomGadget.Activation		= GACT_RELVERIFY | GACT_IMMEDIATE;
	ZoomGadget.GadgetType		= GTYP_BOOLGADGET;
	ZoomGadget.GadgetRender		= &buttonBorder; 
	ZoomGadget.SelectRender		= NULL;
	ZoomGadget.GadgetText		= NULL;
	ZoomGadget.GadgetID			= ZOOM_GADGET;
	ZoomGadget.SpecialInfo		= NULL;
	ZoomGadget.UserData			= NULL;
}

void InitGadgets(void) {
	/* init all gadgets */
 	InitScrollerGadgets(myscreen);
	InitZoomGadget(myscreen);
}

/* simple interface for scrollers with delta values */
VOID SlideBitmap(WORD Dx,WORD Dy) {
	ScrollLayer(0,Win->RPort->Layer,Dx,Dy);
}


/* process currently selected gadget, called from IDCMP_INTUITICKS and when the gadget is released
 * IDCMP_GADGETUP 
 */
VOID CheckGadget(UWORD gadgetID) {
	ULONG tmp;
	WORD dX = 0;
	WORD dY = 0;

	switch (gadgetID) {
    		case UP_DOWN_GADGET:
        		tmp = HEIGHT_SUPER - Win->GZZHeight;
        		tmp = tmp * SideGadInfo.VertPot;
        		tmp = tmp / MAXPROPVAL;
        		dY = tmp - LAYERYOFFSET(Win);
        		break;
    		case LEFT_RIGHT_GADGET:
        		tmp = WIDTH_SUPER - Win->GZZWidth;
        		tmp = tmp * BotGadInfo.HorizPot;
        		tmp = tmp / MAXPROPVAL;
        		dX = tmp - LAYERXOFFSET(Win);
        	break;
    	}
	if (dX || dY) SlideBitmap(dX,dY);
}

/* update gadgets and bitmap positioning when the size changes */
VOID AdjustGadgetsToNewSize(void) {
	ULONG tmp;

	tmp = LAYERXOFFSET(Win) + Win->GZZWidth;
	if (tmp >= WIDTH_SUPER) SlideBitmap(WIDTH_SUPER-tmp,0);

	NewModifyProp(&(BotGad),Win,NULL,AUTOKNOB | FREEHORIZ, 
		((LAYERXOFFSET(Win) * MAXPROPVAL) / (WIDTH_SUPER - Win->GZZWidth)),
    		NULL,
    		((Win->GZZWidth * MAXPROPVAL) / WIDTH_SUPER),
    		MAXPROPVAL,
    		1);

	tmp = LAYERYOFFSET(Win) + Win->GZZHeight;
	if (tmp >= HEIGHT_SUPER) SlideBitmap(0,HEIGHT_SUPER-tmp);

	NewModifyProp(&(SideGad),Win,NULL,AUTOKNOB | FREEVERT,
    		NULL,
    		((LAYERYOFFSET(Win) * MAXPROPVAL) /
        	(HEIGHT_SUPER - Win->GZZHeight)),
    		MAXPROPVAL,
    		((Win->GZZHeight * MAXPROPVAL) / HEIGHT_SUPER),
    		1);
			
	/* adjust ZoomGadget width/height so that scroll bars are usable */
	/* (if ZoomGadget is too big and overlaps scrollers, mousclicks on scrollbars
	 * are directed to ZoomGadgets, i.e. they don't reach scroll bars. Another solution
	 * would probably be to change order of gadgets: first the scrollbars that point to
	 * the ZoomGadget. In this case, the ZoomGadget probably wouldn't need to be resized.
	 */
	ZoomGadget.Width= Win->GZZWidth;
	ZoomGadget.Height= Win->GZZHeight;
}

/* superbitmap functions */

/* handle zoom gadget */
void DrawZoomRectangle(void) {
	SetAPen(rp,1);
	Move(rp,(long)zoom[0],(long)zoom[1]);
	Draw(rp,(long)zoom[2],(long)zoom[1]);
	Draw(rp,(long)zoom[2],(long)zoom[3]);
	Draw(rp,(long)zoom[0],(long)zoom[3]);
	Draw(rp,(long)zoom[0],(long)zoom[1]);
}

void ProcessZoomGadget(struct IntuiMessage* msg) {
	unsigned long wait;
	tmp = HEIGHT_SUPER - Win->GZZHeight;
	tmp = tmp * SideGadInfo.VertPot;
	tmp = tmp / MAXPROPVAL;
	dY = tmp;//tmp - LAYERYOFFSET(Win);
        		
	tmp = WIDTH_SUPER - Win->GZZWidth;
	tmp = tmp * BotGadInfo.HorizPot;
	tmp = tmp / MAXPROPVAL;
	dX = tmp;//tmp - LAYERXOFFSET(Win);
			
	printf("Client: select new region\n");
		
	tx=msg->MouseX-Win->BorderLeft+dX;
	ty=msg->MouseY-Win->BorderTop+dY;
					
	if ((tx<Win->GZZWidth) && (tx>0) && (ty<Win->GZZHeight) && (ty>0)) {
		printf("CLIENT: Mousedown inside GZZ (tx=%lf, ty=%lf).\n",tx,ty);

		zoom[zoom_point]=tx;
		zoom[zoom_point+1]=ty;

		if (zoom_point==0) zoom_point+=2;
		else {
			/* start new calculation */
			zoom_point=0;
			last_update_y=0;
			BlockSubtaskDrawing();
			DrawZoomRectangle();
			for (wait=0;wait<300000;wait++);
			MandelbrotZoomP1P2(zoom[0],zoom[1],zoom[2],zoom[3]);	
			SetAPen(rp,0);
			RectFill(rp,0,0,800,600);	
			AllowServerDrawing();
		}			
	}
}

void UpdateScreen(void) {
	int x,y;
	BlockSubtaskDrawing();
	new_update_y=*pay; // read directly server memory
	if (new_update_y>last_update_y) {
	 	for (y=last_update_y;y<new_update_y;y++) {
			for (x=0;x<WIDTH_SUPER;x++) {
				SetAPen(rp,ServerBitmapPtr[(y*WIDTH_SUPER) + x ]);
				WritePixel(rp,x,y);
			}
	 	}
	 	last_update_y=new_update_y;
	}
	AllowServerDrawing();						
}

void ProcessMenuSelection(UWORD menuNum, UWORD itemNum, UWORD subNum) {
	if (menuNum==0) {
			switch (itemNum) {
					case 0 :
						/* set new maxit */
						printf("CLIENT: Set new maximum for iterations (not implemented).\n");
						break;
					case 1 : 
						/* update screen */ 
						UpdateScreen();
						break;
                    case 2 : 
						/* zoom+ */
						MandelbrotZoom(0.2,-0.2,0.15,-0.15);	
						SetAPen(rp,0);
						RectFill(rp,0,0,800,600);
						break;
                    case 3 : 
						/* zoom- */
						MandelbrotZoom(-0.2,0.2,-0.15,0.15); 
						SetAPen(rp,0);
						RectFill(rp,0,0,800,600);
						break;

			}
	}
}


/* main message loop */
VOID ProcessMsgLoop(void) {
	struct IntuiMessage *msg;
	WORD flag = TRUE;
	UWORD currentGadget = NO_GADGET;
	eventcounter=0;
	last_update_y=0;
	
	while (flag) {
    	WaitPort(Win->UserPort);
	
    	while (msg = (struct IntuiMessage *)GetMsg(Win->UserPort)) {
			msgClass = msg->Class;
       		menuNumber = msg->Code;
            
    		switch (msg->Class) {
            		case IDCMP_CLOSEWINDOW:
                			flag = FALSE;
							closewin = TRUE;
							BlockSubtaskDrawing();
                			break;
            		case IDCMP_NEWSIZE:
                			AdjustGadgetsToNewSize();
                			break;
            		case IDCMP_GADGETDOWN:
                			currentGadget = GADGETID(msg);
							switch (currentGadget) {
								case ZOOM_GADGET : ProcessZoomGadget(msg); break;
                			}
							break;
            		case IDCMP_GADGETUP:
                			CheckGadget(currentGadget);
                			currentGadget = NO_GADGET;
                			break;
            		case IDCMP_INTUITICKS:
                			CheckGadget(currentGadget);
                			break;	
					case IDCMP_MENUPICK:
							/* get menu, item, subitem */
		    				menuNum = MENUNUM(menuNumber); 
                    		itemNum = ITEMNUM(menuNumber);
                    		subNum = SUBNUM(menuNumber);
               				if (menuNum == 0 && itemNum == 4) {
								flag=FALSE; // quit
								closewin = TRUE;
								BlockSubtaskDrawing();
							} else ProcessMenuSelection(menuNum,itemNum,subNum);		
							break;
            }
        	ReplyMsg((struct Message *)msg);
    	}
	}
}

/* create, initialize and process the super bitmap window  */
VOID CreateAndStartSuperBitmapWindow(struct Screen *myscreen) {
	struct BitMap *bigBitMap;
	WORD planeNum;
	WORD allocatedBitMaps;

	InitGadgets();
	
	/* allocate memory for superbitmap */
	/* MEMF_CLEAR flag => all bitmap pointers are NULL, except those successfully allocated */
	if (bigBitMap=AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR)) {
			memory_alloc=sizeof(struct BitMap);
			memory_total+=memory_alloc;
			printf("CLIENT: MEMORY: total=%d (+%d) [CrateAndStartSuperBitmapWindow()]]\n",memory_total,memory_alloc);
    		InitBitMap(bigBitMap, myscreen->BitMap.Depth, WIDTH_SUPER, HEIGHT_SUPER);

    		allocatedBitMaps = TRUE;
    		for (planeNum=0; (planeNum < myscreen->BitMap.Depth) && (allocatedBitMaps == TRUE); planeNum++) {
        		bigBitMap->Planes[planeNum] = AllocRaster(WIDTH_SUPER, HEIGHT_SUPER);
				memory_alloc=(WIDTH_SUPER*HEIGHT_SUPER)/8;
				memory_total+=memory_alloc;
				printf("CLIENT: MEMORY: total=%d (+%d) [CreateAndStartSuperBitmapWindow()]\n", memory_total, memory_alloc);
				
        		if (NULL == bigBitMap->Planes[planeNum]) allocatedBitMaps = FALSE;
        	}

    		/* allocation sucessful => open window (or fail silently) */
    		if (TRUE == allocatedBitMaps) {
        		if (NULL != (Win = OpenWindowTags(NULL,
                				WA_MinWidth, 50,
            					WA_MinHeight, 50,
            					WA_MaxWidth, 800,
            					WA_MaxHeight,600,
						WA_Width,  400,
                				WA_Height, 300,
                				WA_MaxWidth,  WIDTH_SUPER,
                				WA_MaxHeight, HEIGHT_SUPER,
                				WA_IDCMP, IDCMP_GADGETUP | IDCMP_GADGETDOWN |
                    					IDCMP_NEWSIZE | IDCMP_INTUITICKS | 
							IDCMP_CLOSEWINDOW | IDCMP_MENUPICK |
							IDCMP_MOUSEBUTTONS,
                				WA_Flags, WFLG_SIZEGADGET | WFLG_SIZEBRIGHT | WFLG_SIZEBBOTTOM |
                    				WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                    				WFLG_SUPER_BITMAP | WFLG_GIMMEZEROZERO | WFLG_NOCAREREFRESH |
						WFLG_ACTIVATE,
                				WA_Gadgets, &(ZoomGadget), // ZoomGadget points to &SideGad
		        				WA_Title, "Amiga rulez ... :)",
                				WA_PubScreen, myscreen,
                				WA_SuperBitMap, bigBitMap,
                				TAG_DONE))) {
				
						/* OpenWindowTags() successful */
						/* now attach menu to window */
        				SetMenuStrip (Win, &menu1);
            			/* init window display */
            			SetRast(Win->RPort,0); /* clear bitplanes */
            			SetDrMd(Win->RPort,JAM1);
				
						/* get windows rastport & inner_width/height */
        				rp=Win->RPort;
        				inner_width=Win->GZZWidth;
        				inner_height=Win->GZZHeight;
	
	    				AdjustGadgetsToNewSize();

            			/* process the window, return on IDCMP_CLOSEWINDOW */
            			ProcessMsgLoop();

            			CloseWindow(Win);
        		}
        	}

    		printf("CLIENT: Free client window superbitmap rasters.\n");
			for (planeNum=0; planeNum<myscreen->BitMap.Depth; planeNum++) {
        		/* free only the bitplanes actually allocated */
        		if (NULL != bigBitMap->Planes[planeNum]) {
					FreeRaster(bigBitMap->Planes[planeNum], WIDTH_SUPER, HEIGHT_SUPER);
					memory_alloc=(WIDTH_SUPER*HEIGHT_SUPER)/8;
					memory_total-=memory_alloc;
					printf ("CLIENT: MEMORY: total=%d (-%d) [CreateAndStartSuperBitmapWindow()]\n", memory_total, memory_alloc);
				
        		}
			}
    		FreeMem(bigBitMap,sizeof(struct BitMap));
			memory_alloc=sizeof(struct BitMap);
			memory_total-=memory_alloc;
			printf("CLIENT: MEMORY: total=%d (-%d) [CreateAndStartSuperBitmapWindow()]\n",memory_total,memory_alloc);
    		
    	}
}

/* message ports */

BOOL FindServerPort(void) {
	printf("CLIENT: Searching for public port \"%s\" ...\n",PortName);
	Forbid(); // necessary?
    myMsgPort=FindPort(PortName);
    Permit();
    if (myMsgPort) {
		printf("CLIENT: Success - found public server port!\n");
		return TRUE;
    } else {
		printf("CLIENT: Mandelbrot server hasn't been started.\n");
		printf("CLIENT: Start it from shell using:\n");
		printf("CLIENT: run mandelbrot_server <return>\n");
		return FALSE;
	}
}

void DeallocateClientMsg(void) {
	printf("CLIENT: Free message memory.\n");
    FreeMem(ClientMsg,sizeof(struct MandelbrotMsg));
	memory_alloc=sizeof(struct MandelbrotMsg);
	memory_total-=memory_alloc;
	printf("CLIENT: MEMORY: total=%d (-%d) [DeallocateClientMsg()]\n",memory_total,memory_alloc);		
}

BOOL AllocateClientMsg(void) {
	ClientMsg=(struct MandelbrotMsg*)(void*)AllocMem(sizeof(struct MandelbrotMsg), MEMF_PUBLIC | MEMF_CLEAR);
	memory_alloc=sizeof(struct MandelbrotMsg);
	memory_total+=memory_alloc;
	printf("CLIENT: MEMORY: total=%d (+%d) [AllocateClientMsg()]\n",memory_total,memory_alloc);
    		
	if (ClientMsg) {
		printf("CLIENT: Memory for client message allocated.\n");	
		return TRUE;
	} else {
		printf("CLIENT: Couldn't allocate memory for client message.\n");
		return FALSE;        
	}
}

VOID DeleteClientPort(void) {
	printf("CLIENT: Delete reply port.\n");
    DeletePort(myReplyPort);
}

BOOL CreateReplyPort(void) {
	printf("CLIENT: Create reply port.\n");
    myReplyPort=CreatePort("Reply",0);
    if (myReplyPort) {
		printf("CLIENT: Success - reply port created!\n");
        if (AllocateClientMsg() == FALSE) {
			DeleteClientPort();
			return FALSE;	
		} else return TRUE;
	} return FALSE;
}

BOOL InitMessagePorts(void) {
    if (FindServerPort()) {   
		if (CreateReplyPort()) return TRUE;
		else return FALSE;
	} else return FALSE;		
}

void SendCloseMsgToServer(void) {
	printf("CLIENT: Main program closing -> Send REQ_QUIT to server and wait for answer...\n");
	ClientMsg->type=REQ_QUIT;
	Forbid();
	PutMsg(myMsgPort,(struct Message*)(void*)ClientMsg);
	Permit();
	WaitPort(myReplyPort);
	do {
		ServerMsg=(struct MandelbrotMsg*)(void*)GetMsg(myReplyPort);
        	if (ServerMsg->type == RPL_QUIT) printf("CLIENT: Server has confirmed quit message -> quit.\n");
		else printf("CLIENT: Server message type is=%d\n", ServerMsg->type);
	} while (ServerMsg->type!=RPL_QUIT);	
}

/* messages */

void MakeInitialConnectionToServer(void) {
	/* each communication will be initiated by client (server will only respond) */
	/* to identify clearly a message id will be used */
	/* client will keep track of this global variable */
	message_id=1;
	/* compose 1st message: connect and start calculation */
	ClientMsg->type=REQ_CONN_AND_CALC_START;
	ClientMsg->id=message_id;
	ClientMsg->x1=minx;
	ClientMsg->y1=miny;
	ClientMsg->x2=maxx;
	ClientMsg->y2=maxy;
	ClientMsg->resx=WIDTH_SUPER;
	ClientMsg->resy=HEIGHT_SUPER;
	ClientMsg->mn.mn_ReplyPort=myReplyPort;
	ClientMsg->rp=&rp;
	ClientMsg->draw=&allow_drawing;
	PutMsg(myMsgPort,(struct Message*)(void*)ClientMsg);
	WaitPort(myReplyPort);
	ServerMsg=(struct MandelbrotMsg*)(void*)GetMsg(myReplyPort);
        
	if (ServerMsg->type != RPL_CONN_AND_CALC_START) {
		printf("CLIENT: error connecting to server -> quit.\n");
	}
	
	message_id++;
	ServerBitmapPtr=ServerMsg->data;
	pax=ServerMsg->pax;
	pay=ServerMsg->pay;
	printf("CLIENT: PTRS: ServerBitmap: %d ax: %d ay: %d\n",ServerBitmapPtr, pax, pay);
}

/* libraries */

BOOL OpenAllLibraries(void) {
	/* open libraries (intuition minimum version = 37) */
	if (IntuitionBase=OpenLibrary("intuition.library",37L)) {
    		if (GfxBase=OpenLibrary("graphics.library",33L)) {
        			if (LayersBase=OpenLibrary("layers.library",33L)) return TRUE;
					else {
						CloseLibrary(GfxBase);
						CloseLibrary(IntuitionBase);
						printf("CLIENT: Couldn't open layers.library.\n");	
						return FALSE;
					}
			} else {
					CloseLibrary(IntuitionBase);
					printf("CLIENT: Couldn't open intuition.library.\n");
					return FALSE;
			}
	} else return FALSE;
}

/* main */
int main(int argc, char **argv) {

	printf("CLIENT: Starting up mandelbrot client ...\n");
	
	if (InitMessagePorts()==FALSE) exit(1);
	InitMandelbrotValues();
	MakeInitialConnectionToServer();
	AllowServerDrawing();
	
	if (OpenAllLibraries() == FALSE) exit(1);	

	
	/* get wbscreen ptr */
    if (NULL!=(myscreen=LockPubScreen(NULL))) {
     		CreateAndStartSuperBitmapWindow(myscreen);
            UnlockPubScreen(NULL,myscreen);
    } 
	
	/* no public screen or program ended => close all libraries */
	printf("CLIENT: Close libraries.\n");
	CloseLibrary(LayersBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
 
	/* terminate program */
	SendCloseMsgToServer();
	DeallocateClientMsg(); 	
    DeleteClientPort();		
}



