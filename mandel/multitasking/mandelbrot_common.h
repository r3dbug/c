
/* includes for mandelbrot_server and mandelbrot_client */

#define SERVER_MAXX 1280
#define SERVER_MAXY 800

#define REQ_CONNECT 		1
#define RPL_CONNECT 		2
#define REQ_CONN_AND_CALC_START 3
#define RPL_CONN_AND_CALC_START 4
#define REQ_CALC_START 		5
#define RPL_CALC_START 		6
#define REQ_QUIT 		7
#define RPL_QUIT 		8
#define MSG_ERROR 		9999;

struct MandelbrotMsg {
        struct Message mn;
        unsigned int type;
		unsigned int id;
		double x1, y1, x2, y2;
        long resx, resy;
        void *data;
		double *pax, *pay;
	    UBYTE *draw; 		// allow drawing (0 = server can't draw, 1 = server is allowed to draw) 
		struct RastPort** rp;
};

unsigned int message_id=0;
