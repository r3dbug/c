/*  Vampire V4 ApolloWheel - (c) ApolloTeam 2023
 *  Based on WheelBusMouse and ps2m released under GPL:
 *  - https://aminet.net/package/docs/hard/ps2m
 *  - https://aminet.net/package/util/mouse/WheelBusMouse
 *  Adapted and extended for Vampire by RedBug.
 */


/* KNOWN ISSUES:
 * => When stopped/restarted ApolloWheel looses about 15KB of FastMem
 * => Workaround: use STOP/RESTART options only for configuring
 * (after that, start ApolloWheel once and keep it running)
 */

#include <stdio.h>
#include <stdlib.h>
#include <exec/memory.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <clib/input_protos.h>
#include <clib/graphics_protos.h>
#include <clib/alib_protos.h>
#include <pragmas/exec_sysbase_pragmas.h>
#include <pragmas/graphics_pragmas.h>
#include <pragmas/input_pragmas.h>
#include <pragmas/dos_pragmas.h>
#include <dos/dostags.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/startup.h>
#include <proto/icon.h> 
#include <devices/input.h>
#include <devices/inputevent.h>
#include "newmouse.h"

/* Date & Version */
#define DATE 		"2023-06-16"
#define VERSION		"0.1i (Beta)"

UBYTE versiontag[] = "\0$VER: ApolloWheel"VERSION" ("DATE") by RedBug";

/* modes */
#define MODES 2
#define METHODS 3

/* RAWKEY EVENTS */
#define AW_CURSORUP     0x4c
#define AW_CURSORDOWN	0x4d
#define PAGEUP		    0x48
#define PAGEDOWN	    0x49

/* register $dff212 = wheelcounter (BYTE) [cores >9435] */
#define WHEELCOUNTER    (*((UBYTE*)0xDFF212+1))         	/* new register: wheelcounter */
#define MIDDLEBUTTON 	(!(*((UWORD*)0xDFF016) & (1<<8)))	/* POTGOR */
#define LEFTBUTTON 		(!(*(UBYTE*)0xBFE001 & (1<<6)))		/* CIAPRA */
#define RIGHTBUTTON 	(!(*(UWORD*)0xDFF016 & (1<<10)))	/* POTGOR */
#define FOURTHBUTTON    ((*((UBYTE*)0xDFF212)) & 1)			/* test - logitech ID0162 thumb button 1*/
#define FIFTHBUTTON		((*((UBYTE*)0xDFF212)) & 2)			/* test -    "        "   thumb button 2*/
#define DOUBLETOGGLE    ((LEFTBUTTON) && (RIGHTBUTTON))     /* toggle by clicking on left+right button simultaneously */

/* default delay */
#define DEFAULTDELAY 2

/* command line arguments */
#define TEMPLATE "REVERSEH/S,REVERSEV/S,NOMIDDLETOGGLE/S,NODOUBLETOGGLE/S,NOTOGGLE/S,DELAY/N,WAITTOF/N,MULTIPLY/N,DIVIDE/N,MODE0/K,MODE1/K,SELECT/N,PRIORITY/N,QUIET/S,ALLQUIET/S,STOP/S,RESTART/S,HELP/S"

/* global variables */
char template_str[] = TEMPLATE;

extern struct Library *DOSBase;
extern struct Library *SysBase;
struct Library *GfxBase, *InputBase;
struct Library* iconbase;

struct IOStdReq *req;
struct InputEvent FakeEvent;

struct {
	ULONG reverseh;
	ULONG reversev;
	ULONG nomiddletoggle;
	ULONG nodoubletoggle;
	ULONG notoggle;
	ULONG wait;
	ULONG waittof;					/* use WaitTOF (Top of Frame) or not (no value = default = 1 WaitTOF) */
	ULONG multiply;
	ULONG divide;
	UBYTE *mode0;
	UBYTE *mode1;
	ULONG select;
	LONG  *pri;
	ULONG quiet;
	ULONG allquiet;
	ULONG stop;
	ULONG restart;
	ULONG help;
} arg = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

struct RDArgs *rda;
struct Task *act_task; 			/* Process */
struct Task *launched_task;		/* Process */

struct {
	UBYTE delta;
	UBYTE factor;
	void (*method)(LONG);
} mode[MODES][METHODS];

struct WBArg** arglist;
struct WBStartup* argmsg;
struct WBArg *wb_arg;
int arg_num;
char **ttypes;
LONG ttvalues[20]; 				/* ToolType values */
UBYTE from_shell;
char TaskName[] = "ApolloWheelUSB " VERSION;
ULONG code;
ULONG wcount;

/* prototypes */
void Event(LONG);
void ResidentProcess(void);
void PrintArgs(void);
void PrintHelp(void);
void ParseModes(void);
void WaitButtons(void);
void NewMouseWheelRightLeftEvent(LONG);
void ParseToolTypeParam(int,char*,char);
struct RDArgs * ReadArgsOrToolTypes(int, char**);
BYTE LaunchedFromShell(void);
void PrintInfoAndArgs(void);
void SetStopWithRestart(void);
void SetQuietWithAllQuiet(void);
void StopRunningApolloWheel(struct Task*);
void StartApolloWheel(void);
void RestartApolloWheel(void);
void SetNoToggleWithBothTogglesDisabled(void);

// void fbutton(void);


/* main function */

int __saveds main(int argc, char** argv)
{
	
    SysBase = *((struct Library **)4L);
	
	if(DOSBase = OpenLibrary("dos.library",0))
	{
		if(GfxBase = OpenLibrary("graphics.library",36))
		{
			from_shell = LaunchedFromShell();
			rda = ReadArgsOrToolTypes(argc,argv);
			
			if(rda) 
			{
				
				SetQuietWithAllQuiet();
				SetNoToggleWithBothTogglesDisabled();
				ParseModes();
				PrintInfoAndArgs();
				SetStopWithRestart();
		
		    	/* activate wheel mouse functionality */
				if (arg.help) PrintHelp();
				else
				{
					if (act_task=FindTask(TaskName))
					{

						if (arg.stop)
						{
							StopRunningApolloWheel(act_task);
						
							/* restart */
							if (arg.restart) 
							{
								RestartApolloWheel();
								if (from_shell) return 0;
							}
						} 
						else if (!arg.allquiet) 
						{
							
							printf("ApolloWheel already running - use STOP or RESTART!\n");
							printf("Task number: %d\n",((struct Process*)act_task)->pr_CLI);
						}
					}
					else					
					{
						if ((!arg.stop) || (arg.restart))
						{
							StartApolloWheel();
							if (from_shell) return 0;
						} else if (!arg.quiet) printf("You must first start ApolloWheel before you can stop it.\n");
					}
				}
				CloseLibrary(GfxBase);
			} else printf("Error parsing arguments.\n");
		}
		else
		{
			Write(Output(),"OS 2.0+ required\n",17);
		}
		CloseLibrary(DOSBase);
	}
	return 20;  /* 20 = error code when openlibrary fails ?! */
}

void __saveds ResidentProcess(void)
{
	struct MsgPort *replyport;
	
	LONG old_wy;
	LONG act_wy;
 
	LONG diff_raw;    
    LONG diff_wy;
	LONG abs_wy;

	UBYTE act_mode;
	UBYTE n;
	
	LONG act_sig;
	
	act_mode = (arg.select) ? *(ULONG*)arg.select : 0;
		
	if(replyport = CreateMsgPort())
	{
		if(req = CreateIORequest(replyport,sizeof(struct IOStdReq)))
		{
			if(!OpenDevice("input.device",NULL,(struct IORequest *)req,NULL))
			{		
                old_wy = act_wy = WHEELCOUNTER;

				InputBase = (struct Library *)req->io_Device;
				while(!(act_sig=CheckSignal(0xf000)))
				{
					/* default delay>0 is necessary for Coffin & AmigaOS */
					if (arg.wait) 
					{
						/* avoid Delay(0) because it had issues in first version 1.x of AmigaOS */
						if (*((ULONG*)(arg.wait))!=0) Delay(*(ULONG*)arg.wait);
					} else Delay(DEFAULTDELAY);
					
				
					/* toggle mode if MIDDELBUTTON or LEFTBUTTON+RIGHTBUTTON is pressed */
					if (!arg.notoggle) 
					{
						if (
						   ((MIDDLEBUTTON) && (!arg.nomiddletoggle))
						   ||
						   ((DOUBLETOGGLE) && (!arg.nodoubletoggle))
						   )
						{
							act_mode = (act_mode + 1) & 1;
							WaitButtons();
						
							/* don't click and scroll => reset old_wy / act_wy */
							act_wy = old_wy = WHEELCOUNTER;		
						}
					}
						
					/* read act wheelcounter and do the scrolling */
                    act_wy = WHEELCOUNTER;
                    	

					if (act_wy != old_wy) 
					{
						/* calculate raw difference */
						diff_raw = act_wy - old_wy;
					
						/* calculate diff_wy (if diff_raw > +/- 127, register has wrapped around */
						if (diff_raw<-127) diff_wy = diff_raw + 255;
                   		else if (diff_raw>127) diff_wy = diff_raw - 255;
                   		else diff_wy = diff_raw;
                		
						/* reverse vertical scrolling direction (if selected) */
                   		if (arg.reversev) diff_wy = -diff_wy;
					
						/* calculate absolute diff_wy to check for limit  => maybe this check is not necessary (?!) */
						abs_wy = (diff_wy<0) ? -diff_wy : diff_wy;
				
						/* multiply diff_wy (if selected) */
                   		if (arg.multiply) diff_wy*=*(ULONG*)arg.multiply;
					
						/* divide diff_wy (if selected) */
						if (arg.divide) diff_wy/=*(ULONG*)arg.divide;


						/* generate events */
						n=0;
						while ((n<METHODS) && (mode[act_mode][n].method))  
						{
							if (abs_wy >= mode[act_mode][n].delta) 
							{
								diff_wy *= mode[act_mode][n].factor;
								mode[act_mode][n].method(diff_wy);
							}
							n++;
						}
						
						old_wy = act_wy;
					}
					
					/* horizontal scrolling (mapped to buttons 4+5 for the moment) */
					if (FOURTHBUTTON) {
						if (arg.reverseh) code=NM_WHEEL_RIGHT;
						else code = NM_WHEEL_LEFT;
						NewMouseWheelRightLeftEvent(1);	
					} else if (FIFTHBUTTON) {
						if (arg.reverseh) code=NM_WHEEL_LEFT;
						else code = NM_WHEEL_RIGHT;
						NewMouseWheelRightLeftEvent(1);	
					}
						
					/* WaitTOF:
					   - works well on classic PAL screens or Cybergraphics (ApolloOS)
					   - does not work (reliably) on Coffin and AmigaOS 3.2 (with P96)
					   SOLUTION: 
					   - give the user the possibility to activate / disactivate WaitTOF()
					   - no argument for WAITOF => 1 WaitTOF() (DEFAULT)
					   - waittof=0: no WaitTOF()
					   - waittof=n: n WaitTOF()s [2 WaitTOF()s might be necessary for interlaced screens
					*/
				    if (arg.waittof) 
					{
						if (*((ULONG*)(arg.waittof))!=0) 
						{
							wcount=*((ULONG*)(arg.waittof)); 
							while (wcount--) WaitTOF();   	
						}
					} else WaitTOF();
				}
				CloseDevice((struct IORequest *)req);
			}
			DeleteIORequest(req);
		}

		DeleteMsgPort(replyport);
	}
	CloseLibrary(GfxBase);
	CloseLibrary(DOSBase);
	RemTask(0L);
}

/*
void fbutton(void)
{
	code = NM_BUTTON_FOURTH;
	event(IECLASS_RAWKEY);
	event(IECLASS_NEWMOUSE);
}
*/

void CursorUpDownEvent(LONG diff_wy) {
	LONG count;
	code = (diff_wy>0) ? AW_CURSORUP : AW_CURSORDOWN;
	count = diff_wy < 0 ? -diff_wy : diff_wy;
	while(count--)
	{
		Event(IECLASS_RAWKEY);
	}
}

void PageUpDownEvent(LONG diff_wy) {
	LONG count;
	code = (diff_wy>0) ? PAGEUP : PAGEDOWN;
	count = diff_wy < 0 ? -diff_wy : diff_wy;
	while(count--)
	{
		Event(IECLASS_RAWKEY);
	}
}

void NewMouseWheelUpDownEvent(LONG diff_wy) {
	LONG count;
	code = (diff_wy>0) ? NM_WHEEL_UP : NM_WHEEL_DOWN;						      
	count = diff_wy < 0 ? -diff_wy : diff_wy;
	while(count--)
	{
		Event(IECLASS_RAWKEY);
		Event(IECLASS_NEWMOUSE);
	}
}

void NewMouseWheelRightLeftEvent(LONG count) {
	while(count--)
	{
		Event(IECLASS_RAWKEY);
		Event(IECLASS_NEWMOUSE);
	}

}

void Event(LONG class)
{
	FakeEvent.ie_NextEvent = NULL;
	FakeEvent.ie_Class = class;
	FakeEvent.ie_Code = code;
	FakeEvent.ie_Qualifier = NULL;
	req->io_Data = (APTR)&FakeEvent;
	req->io_Length = sizeof(struct InputEvent);
	req->io_Command = IND_WRITEEVENT;
	DoIO((struct IORequest *)req);
}

void WaitButtons(void) {
	while ((MIDDLEBUTTON) || ((LEFTBUTTON) && (RIGHTBUTTON))) Delay(1);
}

void SetDefaultModes(void) {
	/* set defaults */
	/* mode 0 */
	/* method 0 */
	mode[0][0].delta = 1; 
	mode[0][0].factor = 1;
	mode[0][0].method = NewMouseWheelUpDownEvent;
	/* method 1 */
	mode[0][1].delta = 2; 
	mode[0][1].factor = 1;
	mode[0][1].method = NULL; 	
	/* method 2 */
	mode[0][2].delta = 3; 
	mode[0][2].factor = 1;
	mode[0][2].method = NULL;
	/* mode 1 */
	/* method 0 */
	mode[1][0].delta = 1; 
	mode[1][0].factor = 1;
	mode[1][0].method = CursorUpDownEvent;
	/* method 1 */
	mode[1][1].delta = 2; 
	mode[1][1].factor = 1;
	mode[1][1].method = PageUpDownEvent;
	/* method 2 */
	mode[1][2].delta = 3; 
	mode[1][2].factor = 1;
	mode[1][2].method = NewMouseWheelUpDownEvent;
}

int isdigit(UBYTE c) {
	if ((c >= '0') && (c <= '9')) return 1;
	else return 0;
}

void ParseModes(void) {
	UBYTE* d[MODES];
	int m, n, j;

	SetDefaultModes();

	d[0] = arg.mode0;
	d[1] = arg.mode1;

	for (m=0; m<MODES; m++) 
	{
		if (d[m]) 
		{
			j=0;
			for (n=0; n<METHODS; n++) {
				if ((d[m][j]) && (isdigit(d[m][j])))
				{
					mode[m][n].delta=d[m][j]-'0';
//					printf("Mode %d Event %d delta=%d\n",m,n,mode[m][n].delta);
					j++;
				} // else printf("No new delta (use default).\n");
				if ((d[m][j]) && (isdigit(d[m][j])))
				{
					mode[m][n].factor=d[m][j]-'0';
//					printf("Mode %d Event %d factor=%d\n",m,n,mode[m][n].factor);
					j++;	
				} // else printf("No new factor (use default).\n");
				switch (d[m][j]) 
				{
					case 0 : 
//						printf("No new method (NULL).\n");
						mode[m][n].method=NULL;
						break;
					case 'C' :
					case 'c' : 
//						printf("AW_CURSORUpDownEvent\n");
						mode[m][n].method=CursorUpDownEvent;
						j++;
						break;
					case 'P' :
					case 'p' : 
//						printf("PageUpDownEvent\n");
						mode[m][n].method=PageUpDownEvent;
						j++;
						break;
					case 'N' :
					case 'n' :
//						printf("NewMouseWheelEvent\n"); 
						mode[m][n].method=NewMouseWheelUpDownEvent;
						j++;
						break;
					default : 
						j++;	/* ignore error */
						mode[m][n].method=NULL;
						printf("No valid method specified (NULL).\n");
				}
				
			}
		}
	}
}

BYTE LaunchedFromShell(void) 
{
	if (launched_task=FindTask(NULL))
	{
		if (((struct Process*)launched_task)->pr_TaskNum!=0) return 1;
		else return 0;
	}
}

void PrintInfoAndArgs(void) {
	if(!arg.quiet)
	{
		printf("Vampire V4 ApolloWheel for USB mice - (c) ApolloTeam\nVersion: %s Date: %s\n", VERSION, DATE);
		if (!arg.help) printf("Type 'ApolloWheel HELP' for options.\n");
	}
}

void SetStopWithRestart(void) 
{
	if (arg.restart) {
		if ((arg.stop) && (!arg.quiet)) printf("STOP not necessary with RESTART option.\n");	
		else arg.stop=1;
	}			
}

void SetQuietWithAllQuiet(void)
{
	if (arg.allquiet) arg.quiet=1;			
}

void SetNoToggleWithBothTogglesDisabled(void)
{
	if ((arg.nomiddletoggle) && (arg.nodoubletoggle)) arg.notoggle=1;
}


void StopRunningApolloWheel(struct Task* t) 
{
	Signal(t, 0xf000);
	if (!arg.quiet) printf("BREAK signal sent to task %p.\n", t);
}

void StartApolloWheel(void) {
	PrintArgs();
	switch (from_shell) {
		case 1 : /* default priority = 0
				  * recommended priority = 2 because it makes scrolling more fluid 
				  */
				if(CreateNewProcTags(NP_Entry,ResidentProcess,NP_Name,TaskName,NP_Priority,arg.pri?*arg.pri:0,NULL))
				{
					((struct CommandLineInterface *)((((struct Process *)FindTask(NULL))->pr_CLI)<<2))->cli_Module = NULL;
					if (!arg.quiet) printf("ApolloWheel (re)started.\n");
				} else if(!arg.quiet) printf("Can't create daughter process\n");
				break;
					
		default : /* Started from WB */
				  act_task=FindTask(NULL);
			      if (arg.pri) SetTaskPri(act_task, *arg.pri);
			      act_task->tc_Node.ln_Name = TaskName;
				  
				  /* started asynchronously (e.g. in WBStartup) 
				   * => no need to create a new process 
				   * Just add "DONOTWAIT" cookie to icon.
				   */	
				  ResidentProcess();  
	}
}

void RestartApolloWheel(void) 
{
	if (!arg.quiet) printf("Restarting ApolloWheel ...\n");
	StartApolloWheel();
}

void ParseToolTypeParam(int n, char* name, char type) 
{
	char* tempstr;
	ULONG* temp=&(arg.reverseh);
	switch (type) 
	{
		case 'S' : /* switch: pointer != NULL => true */
				   if (FindToolType(ttypes, name)) { /* printf("Switch %s (arg: %d) set\n", name, n); */ temp[n]=1; }
				   else { /* printf("Switch %s (arg: %d) not set\n", name, n); */ temp[n]=0; }
				   break;
		case 'N' : /* numeric: pointer != NULL => value */
				   tempstr=FindToolType(ttypes, name);
				   if (tempstr) 
				   { 
				   		ttvalues[n]=atoi(tempstr);
				   		//printf("Number %s (arg: %d) value = %d\n", name, n, ttvalues[n]); 
						temp[n]=(ULONG)&ttvalues[n]; 
				   }
				   else { /*printf("Number %s (arg: %d) not set\n", name, n);*/ temp[n]=0; }
				   break;
		case 'K' : /* keyword: pointer != NULL => string */
				   tempstr=FindToolType(ttypes, name);
				   if (tempstr) 
				   { 
				   		//printf("Keyword %s (arg: %d) string = %s\n", name, n, tempstr); 
						temp[n]=(ULONG)tempstr; 
				   }
				   else { /*printf("Keyword %s (arg: %d) not set\n", name, n);*/ temp[n]=0; }
				   break;
		
	}
}

/* We are a bit living on the edge here - ReadArgsOrToolTypes assumes that:
   - all fields in struct arg are of size ULONG or similar (= 32 bit)
   - all arguments in template are of type: NAME/T
   - template parameters and fields in struct are in the exact same order
   - arguments are only of type S (switch), K (keywords), N (numeric)
   It probably assumes more things (like: the startup process is NOT interrupted)
   If anything fails, github.ch/SmartReadArgs might be the alternative solution ...
*/
struct RDArgs * ReadArgsOrToolTypes(int argc, char** argv) 
{
	int oldi, acti, argn;
	int cont;
	
	launched_task=FindTask(NULL);
	
	if (from_shell)
	{
		/* launched from shell */	
		return ReadArgs(template_str,(LONG *)&arg,NULL);
	}
	else
	{
		// printf("Program was launched from Workbench.\n");
		if (iconbase=OpenLibrary("icon.library",0))
		{
					
			ttypes=ArgArrayInit(argc,argv);
					
			oldi=acti=argn=0;
			cont=template_str[acti]!=0;
			while (cont)
			{
				if (template_str[acti]==0) 
				{
					/* last argument */
					template_str[acti-2]=0;
//					printf("last argument: %s Type: %c\n", template_str+oldi, template_str[acti-1]);
					ParseToolTypeParam(argn, template_str+oldi, template_str[acti-1]);
					template_str[acti-2]='/';
					cont=0;
				}
				else if (template_str[acti]==',')
				{
					/* next argument */
					template_str[acti-2]=0;
					template_str[acti]=0;
//					printf("next argument: %s Type: %c\n", template_str+oldi, template_str[acti-/1]);
					ParseToolTypeParam(argn, template_str+oldi, template_str[acti-1]);
					template_str[acti-2]='/';
					acti++;
					argn++;
					oldi=acti;
				}
				else acti++;
			}
			//PrintArgs();
			CloseLibrary(iconbase);
			return (struct RDArgs*)1;
		} 
		else 
		{
			if (!arg.allquiet) printf("Couldn't open icon library.\n");
			return 0;
		}
	}
}

void PrintArgs(void) 
{
	if (!arg.quiet) {
		if (arg.reversev) printf("Option: REVERSE scrolling direction.\n");
		if (arg.reverseh) printf("Option: REVERSE scrolling direction.\n");
		if (arg.wait) printf("Option: DELAY = %ld\n", *(ULONG*)arg.wait);		
		if (arg.waittof) printf("Option: WAITTOF = %ld\n", *(ULONG*)arg.waittof);		
		if (arg.multiply) printf("Option: MULTIPLY = %ld\n", *(ULONG*)arg.multiply);
		if (arg.divide) printf("Option: DIVIDE = %ld\n", *(ULONG*)arg.divide);
		if (arg.mode0) printf("Option: MODE0 (default) = %s\n", arg.mode0);	
		if (arg.mode1) printf("Option: MODE1 (toggle) = %s\n", arg.mode1);	
		if (arg.select) printf("Option: SELECT = %d\n", *(ULONG*)arg.select);	
		if (arg.notoggle) printf("Option: NOTOGGLE\n");	
		if (arg.nomiddletoggle) printf("Option: NOMIDDLETOGGLE\n");	
		if (arg.nodoubletoggle) printf("Option: NODOUBLETOGGLE\n");	
		if (arg.pri) printf("Option: PRIORITY = %d\n", *arg.pri);	
	}
}

void PrintHelp(void) {
	printf("\n");
	printf("REVERSEV           Reverse the vertical scrolling direction (wheel)\n");	
	printf("REVERSEH           Reverse the horizontal scrolling direction (buttons left/right)\n");	
	printf("DELAY=n            Sleep time before reading scrolling register again\n");
    printf("WAITTOF=n          Number of WaitTOF (= Wait Top of Frame) before reading register again (default: 1)\n");
	printf("MULTIPLY=n         Global multiplication (= more scrolling distance per wheel step)\n");
	printf("DIVIDE=n           Global division (= less scrolling distance per wheel step)\n");
	printf("PRIORITY=n         Run ApolloWheel with this priority\n");
	printf("MODE0=<string>     Define scrolling mode0 (default: 11N)\n");
	printf("MODE1=<string>     Define scrolling mode1 (default: 11C21P31N)\n");
	printf("SELECT=n           Select mode n (default: 0)\n");
	printf("NOTOGGLE           Disable toggle function (middle mouse button or left+right button)\n");
	printf("NOMIDDLETOGGLE     Disable toggle for middle mouse button\n");
	printf("NODOUBLETOGGLE     Disable toggle for left+right button\n");
	printf("                   => NOMIDDLETOGGLE plus NODOUBLETOGGLE automatically enables NOTOGGLE\n");
	printf("STOP           	   Stop ApolloWheel driver. (Alternative: Send BREAK signal via Scout)\n");
	printf("RESTART            Start or stop and restart ApolloWheel driver (with new options)\n");

	printf("QUIET              No output (ApolloWheel notifies you if it is already running)\n");
	printf("ALLQUIET           Suppress all outputs and warnings\n");
	printf("\nScrolling:\n");
	printf("Letters: C=CURSORUP/DOWN P=PAGEUP/DOWN N=NEWMOUSE_WHEEL_UP/DOWN\n");
	printf("Numbers: 1st=delta / 2nd=multiplier\n");
	printf("\nExamples:\n");
	printf("11N:               delta=1, multiplier=1, NEWMOUSE_WHEELUP/DOWN\n");
	printf("11C21P31N:         1st) delta>=1, multiplier=1, CURSORUP/DOWN\n");
	printf("                   2nd) delta>=2, multiplier=1, PAGEUP/DOWN\n");
	printf("                   3rd) delta>=3, multiplier=1, NEWMOUSE_WHEELUP/DOWN\n");
	printf("Toggling:          Use middle button or simultaneaous click with left+right button\n");
	printf("                   to toggle between mode0 and mode1.\n");
	printf("\nBEST VALUES:       WAITTOF | DELAY | PRIORITY\n");
	printf("---------------------------+-------+---------\n");
	printf("ApolloOS:              1   |    0  |     2\n");
	printf("Coffin:                0   |    1  |     2\n");
	printf("AmigaOS (3.2):         ?   |    ?  |     ?\n");
	printf("Default:    	       1   |    2  |     0\n");

	printf("\nTEMPLATE:\n%s\n",template_str);
}
