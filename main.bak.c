#include <sys/types.h>	// This provides typedefs needed by libgte.h and libgpu.h
#include <stdio.h>	// Not necessary but include it anyway
#include <libetc.h>	// Includes some functions that controls the display
#include <libgte.h>	// GTE header, not really used but libgpu.h depends on it
#include <libgpu.h>	// GPU library header
#include "psxpad.h" // defines for buttons.

#define OTLEN 8
#define X_WIDTH 320
#define Y_HEIGHT 240
// fixed point unit
#define FIXED_POINT_ONE 4096
#define FIXED_POINT_BITS 12
#define FIXED_POINT_MANTISSA 0xfff
// Define environment pairs and buffer counter
DISPENV disp[2];
DRAWENV draw[2];

extern int tim_zapotec[];


int db;
int ot[2][OTLEN];

char pribuff[2][32768];
char *nextpri;

int tim_mode;
RECT tim_prect, tim_crect;
int tim_uoffs, tim_voffs;

u_char padbuff[2][34]; 
SVECTOR player_tri[] =
{
	{   0, -20, 0 },
	{  10,  20, 0 },
	{ -10,  20, 0 },
};



void LoadTexture(u_long *tim, TIM_IMAGE *tparam){
	// Read TIM info
	OpenTIM(tim);
	ReadTIM(tparam);
	
	// Upload image to framebuffer
	LoadImage(tparam->prect, (u_long*)tparam->paddr);
	DrawSync(0);
	
	// Upload CLUT to framebuffer if present
	if (tparam->mode & 0x8) {
		LoadImage(tparam->crect, (u_long*)tparam->caddr);
		DrawSync(0);
	}
}

// a stupid name...
void LoadStuff(void){
	TIM_IMAGE zapotec;
	
	LoadTexture((u_long*)tim_zapotec, &zapotec);
	
	tim_prect = *zapotec.prect;
	tim_crect = *zapotec.crect;
	tim_mode = zapotec.mode;
	
	//Calulate u,v offset for TIMS that are not page aligned
	tim_uoffs = (tim_prect.x % 64) << (2 -(tim_mode & 0x03));
	tim_voffs = (tim_prect.y & 0xff);
}

void init(void)
{
    // Reset GPU and enable interrupts
    ResetGraph(0);
    
    
    // Configures buffer 1 for 320x240 mode (NTSC)
    SetDefDispEnv(&disp[0], 0, 0, X_WIDTH, Y_HEIGHT);
    SetDefDrawEnv(&draw[0], 0, 240, X_WIDTH, Y_HEIGHT);
    
    // Configures buffer 2 for 320x240 mode (NTSC)
    SetDefDispEnv(&disp[1], 0, 240, X_WIDTH, Y_HEIGHT);
    SetDefDrawEnv(&draw[1], 0, 0, X_WIDTH, Y_HEIGHT);
    
    
    // Specifies the clear color of the DRAWENV
    setRGB0(&draw[0], 63, 0, 127);
    setRGB0(&draw[1], 63, 0, 127);
    
    // Enable background clear
    draw[0].isbg = 1;
    draw[1].isbg = 1;
    

    // Make sure db starts with zero
    db = 0;
   
    
    // init the next primitive pointer to the start of buffer 0.
    nextpri = pribuff[0];
    
    //load textures and possibly other stuff
    LoadStuff();
    
    // Load font texture on upper right of VRAM
    FntLoad(960,0);
    // Define a font window of 100 chars covering the whole screen
    FntOpen(0, 8, 320, 224, 0, 100);
    
    // set tpage of lone texture as initial tpage
    draw[0].tpage = getTPage( tim_mode & 0x3, 0, tim_prect.x, tim_prect.y);
    draw[1].tpage = getTPage( tim_mode & 0x3, 0, tim_prect.x, tim_prect.y);
    
    // Apply environments
    //PutDispEnv(&disp[0]);
    PutDrawEnv(&draw[!db]);
    
    InitPAD( padbuff[0], 34, padbuff[1], 34);
    padbuff[0][0] = padbuff[0][1] = 0xff; // init to ffff so no spurious input in the beginning.
    padbuff[1][0] = padbuff[1][1] = 0xff;
    
    
}


void textured_tile(int x, int y){
	SPRT *sprt;
	
	sprt = (SPRT*)nextpri;
	
	setSprt(sprt); //init it
	setXY0(sprt, x, y);
	setWH(sprt, 64, 64);
	setUV0(sprt,
		tim_uoffs,
		tim_voffs);
	setClut(sprt,
		tim_crect.x,
		tim_crect.y);
	setRGB0(sprt,
		128, 128, 128);
	addPrim(ot[db], sprt);
	nextpri += sizeof(SPRT);
}

void tile(int x, int y)
{
	TILE *tile;
	tile = (TILE*)nextpri;
	
	setTile(tile);
	setXY0(tile, x, y);
	setWH(tile, 64, 64);
	setRGB0(tile, 255, 255, 0);
	addPrim(ot[db], tile);
	nextpri += sizeof(TILE);
	
}

void tri(int x, int y, int angle)
{
	
	SVECTOR v[3];
	POLY_F3 *triangle;
	int i;
	
	// rotate and then translate
	for (i = 0; i < 3; i++)
	{
		v[i].vx = (((player_tri[i].vx * ccos( angle ) )
			-(player_tri[i].vy * csin( angle ) ) ) >> FIXED_POINT_BITS) + (x >> FIXED_POINT_BITS);
		v[i].vy = (((player_tri[i].vy * ccos( angle ) )
			-(player_tri[i].vx * csin( angle ) ) ) >> FIXED_POINT_BITS) + (y >> FIXED_POINT_BITS);	
	}
	
	// sort triangle (put it in draw list)
	triangle = (POLY_F3*)nextpri;
	setPolyF3( triangle );
	setRGB0( triangle, 255, 255, 0 );
	setXY3( triangle,
		v[0].vx, v[0].vy, 
		v[1].vx, v[1].vy, 
		v[2].vx, v[2].vy);
	
	addPrim( ot[db], triangle );
	nextpri += sizeof(POLY_F3);
}

void display(void)
{
    // Wait for GPU to finish drawing and V-Blank
    DrawSync(0);
    VSync(0);
    
    
    
    // Apply environments
    PutDispEnv(&disp[db]);
    PutDrawEnv(&draw[db]);
    
    // Enable display
    SetDispMask(1);
    DrawOTag(ot[db]+OTLEN-1);
    
    // Flip buffer counter
    db = !db;
    nextpri = pribuff[db];
}



int main()
{
    int x = 32;
    int y = 32;
    int xdelta = 5;
    int ydelta = 5;
    int boost = 1;
    int p1x = FIXED_POINT_ONE*160;
    int p1y = FIXED_POINT_ONE*120;
    SVECTOR v[3];
    POLY_F3 *tri;
    
    int angle =0;
    
    
    PADTYPE * pad;
    // Initialize graphics and stuff
    init();
    
    StartPAD();
    // Main loop
    
    
    while(1)
    {
    	
    	pad = (PADTYPE*)padbuff[0];
    	if (pad -> stat == 0)
    	{	
    		// only parse when using digital pad, dual-analog and dual-shock.
    		if ( pad -> type == PAD_TYPE_DIGITAL ||
    			pad -> type == PAD_TYPE_ANALOG ||
    			pad -> type == PAD_TYPE_DUAL)
    		{
    			if ( !(pad->btn & PAD_L1)){
    				boost = 5;
    			}
    			else {
    				boost = 1;
    			}
    			if ( !(pad->btn & PAD_UP)){
    				p1x += csin( angle );
    				p1y -= ccos( angle );
    			}
    			else if ( !(pad->btn & PAD_DOWN)){
    				p1x -= csin( angle );
    				p1y += ccos( angle );
    			}
    			if ( !(pad->btn & PAD_LEFT)){
    				angle -= 16;
    			}
    			else if ( !(pad->btn & PAD_RIGHT)){
    				angle += 16;
    			}
    			
    			
    		}
    	}
    	
    	ClearOTagR(ot[db], OTLEN);
    	
    	// Rotate the triangle coordinates based on the player's angle
        // as well as apply the position
        for( i=0; i<3; i++ )
        {
            v[i].vx = (((player_tri[i].vx*ccos( angle ))
                -(player_tri[i].vy*csin( angle )))>>12)+(p1x>>12);
            v[i].vy = (((player_tri[i].vy*ccos( angle ))
                +(player_tri[i].vx*csin( angle )))>>12)+(p1y>>12);
        }
        
        // Sort the player triangle
        tri = (POLY_F3*)nextpri;
        setPolyF3( tri );
        setRGB0( tri, 255, 255, 0 );
        setXY3( tri,
            v[0].vx, v[0].vy,
            v[1].vx, v[1].vy,
            v[2].vx, v[2].vy );
        addPrim( ot[db], tri );
        nextpri += sizeof(POLY_F3);
    	
    	FntPrint("P1x=%d (%d.%d)\n", p1x, (p1x >> FIXED_POINT_BITS), (p1x& FIXED_POINT_MANTISSA) );
    	FntPrint( "P1y=%d (%d.%d)\n", p1y, (p1y >> FIXED_POINT_BITS), (p1y& FIXED_POINT_MANTISSA) );
    	FntPrint( "ANGLE=%d \n", angle);
    	FntFlush( -1 );
    	//textured_tile(p1x ,p1y);
    	
    	/*
    	tile(x, y);
    	
	
    	x = x + xdelta;
    	y = y + ydelta;
    	if (x > X_WIDTH - 65 || x < 1)
    		xdelta *=-1;
	if (y > Y_HEIGHT - 65 || y < 1)
		ydelta *=-1;
    	*/
        display();
    }
    
    return 0;
}
