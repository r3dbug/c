
/* mandelbrot server
 *
 * server that responds to mandelbrot client
 * calculates mandelbrot continuously (with last data received via message port)
 * whereas the client reacts to user input
 *
 * do not run this program alone (only client can terminate server execution)
 *
 * if possible run it from shell:
 *
 * execute mandelbrot_start <return>
 *
 * compile with sas/c (smake):
 *
 * smake all
 *
 */
 
#include <proto/exec.h>
#include <exec/ports.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <clib/layers_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <stdio.h>
#include "mandelbrot_common.h"

/* defines */
#define NOEVENTS (20000)	// time that the server wont react to new messages

/* bitmap for server */
UBYTE* ServerBitmap;

/* message variables */
struct MsgPort *myMsgPort;
char PortName[]="MandelbrotServer";
struct MandelbrotMsg *ClientMsg;
struct MandelbrotMsg *ServerMsg;
double mx1, my1, mx2, my2;
long done=FALSE;
struct RastPort **clientrp;   
struct Screen* myscreen;

unsigned int memory_total=0, memory_alloc=0;

/* mandelbrot variables */
double test=0.75;

double minx, maxx, 
       miny, maxy, 
       ax, ay,
       deltax, deltay,
       fx, fy,
       dist, maxdist,
       cx, cy,
       zxn1, zyn1,
       tx, ty,
       temp1, temp2, temp3;

int    it, maxit,
       stepx, stepy,
       noevents,
       eventcounter,
       calculation_done;
UBYTE  *draw;


  
VOID initMandelbrotVariables(double mx1, double my1, double mx2, double my2, int resx, int resy) {
	//printf("init variables:minx=%lf maxx=%lf miny=%lf maxy=%lf resx=%d resy=%d\n",mx1,mx2,my1,my2,resx,resy);
	calculation_done=FALSE;
	minx=mx1; 
	maxx=mx2;
	miny=my1;
	maxy=my2;
	deltax=maxx-minx;
	deltay=maxy-miny;
	fx=deltax/resx;
	fy=deltay/resy;
	ax=0;
	ay=0;
	maxit=256;
	maxdist=1024;
	stepx=1;
	stepy=1;
	noevents=resx*2;
}

VOID calculateMandelbrotPoint(VOID) {
	printf("SERVER: calculate next set: ax=%lf ay=%lf\n",ax,ay);
	eventcounter=0;
	while (eventcounter<NOEVENTS) {
		if (ax>=800) { ax=0; ay+=stepy; }
		if (ay>=600) {
			calculation_done=TRUE; // calculation is complete
			return;
		}
		it=0;
		cx=ax*fx+minx; 
		cy=ay*fy+miny;
		zxn1=cx;
		zyn1=cy;
		//printf("ax=%lf ay=%lf cx=%lf cy=%lf\n",ax,ay,cx,cy);
		/* main loop */
		do {
			temp1=zxn1*zxn1;
			temp2=zyn1*zyn1;
			dist=temp1+temp2;
			zyn1=2*zxn1*zyn1+cy;
			zxn1=temp1-temp2+cx;	
			it++;
			//printf("z(%d): x=%lf y=%lf dist=%lf\n",it,zxn1,zyn1,dist);
		} while ((dist<maxdist) && (it<=maxit));
        ServerBitmap[(int)ay*800+(int)ax]=it%256;
	
		if (*draw==1) {
			/* rendering allowed */
			if (*clientrp) {
	   			if (((int)ay%2==0) && ((int)ax%2==0)) {
					myscreen=LockPubScreen(NULL);
					SetAPen(*clientrp,it%256);
					WritePixel(*clientrp,(int)ax,(int)ay); // draw directly to window
	    			UnlockPubScreen(NULL,myscreen);
	    		}
			}
	
		}
       	ax+=stepx;
		eventcounter++;
	}
}

BOOL AllocateServerBitmap(void) {
	ServerBitmap=AllocMem(SERVER_MAXX*SERVER_MAXY,MEMF_PUBLIC | MEMF_CLEAR);
    memory_alloc=SERVER_MAXX*SERVER_MAXY;
	memory_total+=memory_alloc;
	printf("SERVER: MEMORY: total=%d (+%d)\n",memory_total,memory_alloc);
    		
	if (ServerBitmap) {
			printf("\nSERVER: Serverbitmap allocated (%d,%d).\n",SERVER_MAXX, SERVER_MAXY);
    		return TRUE;
	} else {
    		printf("SERVER: Memory allocation for server bimap failed.\n");
            return FALSE;
        
	}
}

void DeallocateServerBitmap(void) {
	printf("SERVER: Free server bitmap and quit.\n");
    FreeMem(ServerBitmap, SERVER_MAXX*SERVER_MAXY);
	memory_alloc=SERVER_MAXX*SERVER_MAXY;
	memory_total-=memory_alloc;
	printf("SERVER: MEMORY: total=%d (-%d)\n",memory_total,memory_alloc);		
}

BOOL AllocateServerMsg(void) {
	printf("SERVER: Allocating memory for server message.\n");
	ServerMsg=AllocMem(sizeof(struct MandelbrotMsg),MEMF_PUBLIC | MEMF_CLEAR);
	memory_alloc=sizeof(struct MandelbrotMsg);
	memory_total+=memory_alloc;
	printf("SERVER: MEMORY: total=%d (+%d)\n",memory_total,memory_alloc);
    		
	if (ServerMsg) {
		printf("SERVER: Success: Server message memory allocated.\n");
		return TRUE;
	} else return FALSE;
}

void DeallocateServerMsg(void) {
	printf("SERVER: Free server message.\n");
	FreeMem(ServerMsg,sizeof(struct MandelbrotMsg));
	memory_alloc=sizeof(struct MandelbrotMsg);
	memory_total-=memory_alloc;
	printf("SERVER: MEMORY: total=%d (-%d)\n",memory_total,memory_alloc);		
}


BOOL CreateServerMessagePort(void) {
	printf("SERVER: Attempting to create message port \"%s\"...\n",PortName);
    myMsgPort=CreatePort(PortName,0);
        if (myMsgPort) {
			printf("SERVER: Success!\n");
        	return TRUE;
		} else return FALSE;
}

void DeleteServerMessagePort(void) {
	DeletePort(myMsgPort);
    printf("SERVER: Message port deleted.\n");  
}

BOOL WaitForInitialClientConnection(void) {
	printf("SERVER: Wait for first message (= connect and start calculation).\n");
	WaitPort(myMsgPort);
	ClientMsg=(struct MandelbrotMsg*)(void*)GetMsg(myMsgPort);
        
	switch (ClientMsg->type) {
		case REQ_CONNECT : 
				printf("SERVER: Connect message received.\n");
				/* prepare reply */
				ServerMsg->type=RPL_CONNECT;
				ServerMsg->id=ClientMsg->id;
				ServerMsg->mn.mn_ReplyPort=myMsgPort;
				ServerMsg->data=ServerBitmap;
				ServerMsg->pax=&ax;
				ServerMsg->pay=&ay;
				ServerMsg->rp=ClientMsg->rp;
				ServerMsg->draw=ClientMsg->draw;
				clientrp=ClientMsg->rp;	
				draw=ClientMsg->draw;
				printf("SERVER: clientrp=%d ClientMsg->rp=%d\n",clientrp,ClientMsg->rp);
				Forbid();
               	PutMsg(ClientMsg->mn.mn_ReplyPort, (struct Message*)(void*)ServerMsg);
               	Permit();
				return TRUE;
			break;
		case REQ_CONN_AND_CALC_START: 
				printf("SERVER: Connect and start message received.\n");
				/* prepare reply */
				ServerMsg->type=RPL_CONN_AND_CALC_START;
				ServerMsg->id=ClientMsg->id;
				ServerMsg->mn.mn_ReplyPort=myMsgPort;
				ServerMsg->data=ServerBitmap;
				ServerMsg->pax=&ax;
				ServerMsg->pay=&ay;
				ServerMsg->rp=ClientMsg->rp;
				ServerMsg->draw=ClientMsg->draw;
				clientrp=ClientMsg->rp;	
				draw=ClientMsg->draw;
				printf("SERVER: clientrp=%d ClientMsg->rp=%d\n",clientrp,ClientMsg->rp);
				
				initMandelbrotVariables(ClientMsg->x1, 
						ClientMsg->y1, 
						ClientMsg->x2, 
						ClientMsg->y2, 
						ClientMsg->resx,
						ClientMsg->resy);
               	ServerMsg->x1=ax;
               	ServerMsg->y1=ay;
				printf("SERVER: PTRS: ServerBitmap: %d|%d ax: %d|%d ay: %d|%d\n",ServerBitmap,ServerMsg->data,&ax,ServerMsg->pax,&ay,ServerMsg->pay);
	
               	Forbid();
               	PutMsg(ClientMsg->mn.mn_ReplyPort, (struct Message*)(void*)ServerMsg);
               	Permit();
				printf("SERVER: sent RPL_CONN_AND_CALC_START to client.\n");	
				return TRUE;			
			break;
		default:
			printf("SERVER: Error connecting -> shutdown server.\n");
			return FALSE;
	}       
	
}

BOOL AllocateAll(void) {
	if (AllocateServerBitmap()==FALSE) return 1; 
	if (AllocateServerMsg()==FALSE) {
		DeallocateServerBitmap();
		return FALSE;
	}
	if (CreateServerMessagePort()==FALSE) {
		DeallocateServerMsg();
		return FALSE;
	
	}	
	return TRUE;
}

void DeallocateAll(void) {
	DeleteServerMessagePort();
	DeallocateServerMsg();
	DeallocateServerBitmap();
}

void Process_REQ_QUIT(void) {
	printf("SERVER: message type = REQ_QUIT.\n");
	ServerMsg->type=RPL_QUIT;
	ServerMsg->id=ClientMsg->id;
	ServerMsg->mn.mn_ReplyPort=myMsgPort;
	Forbid(); // really necessary?
	PutMsg(ClientMsg->mn.mn_ReplyPort, (struct Message*)(void*)ServerMsg);
	Permit();
	printf("SERVER: RPL_QUIT confirmed to client.\n");
	done=TRUE;
}

void Process_REQ_CALC_START(void) {
	printf("SERVER: message type = CALC_START.\n");
	ServerMsg->type=RPL_CALC_START;
	ServerMsg->id=ClientMsg->id;
	ServerMsg->mn.mn_ReplyPort=myMsgPort;
	initMandelbrotVariables(ClientMsg->x1, 
							ClientMsg->y1, 
							ClientMsg->x2, 
							ClientMsg->y2, 
							ClientMsg->resx,
							ClientMsg->resy);
	Forbid(); // really necessary?
	PutMsg(ClientMsg->mn.mn_ReplyPort, (struct Message*)(void*)ServerMsg);
	Permit();
	printf("SERVER: RPL_CALC_START confirmed to client.\n");
}

int main(void) {

    if (AllocateAll()==FALSE) return 1;    
	WaitForInitialClientConnection();
	
	
	printf("SERVER: Starting server loop.\n");
                	        	
    while (!done) {
	
        //printf("SERVER: Check message port ...\n");
		ClientMsg=(struct MandelbrotMsg*)(void*)GetMsg(myMsgPort);
        if (ClientMsg) {
			printf("SERVER: Client message received (id=%d, type=%d)).\n",ClientMsg->id,ClientMsg->type);
			switch (ClientMsg->type) {
				case REQ_QUIT : Process_REQ_QUIT();	break;
				case REQ_CALC_START : Process_REQ_CALC_START(); break;
			}
		}
		if (!calculation_done) calculateMandelbrotPoint();	 
    }
        
    printf("SERVER: Shutting down server after REQ_QUIT / RPL_QUIT...\n");
	DeallocateAll();
}