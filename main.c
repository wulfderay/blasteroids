#include <sys/types.h> // This provides typedefs needed by libgte.h and libgpu.h
#include <stdio.h>	   // Not necessary but include it anyway
#include <libetc.h>	   // Includes some functions that controls the display
#include <libgte.h>	   // GTE header, not really used but libgpu.h depends on it
#include <libgpu.h>	   // GPU library header
#include <libcd.h>	   // cdrom utils
#include <libapi.h>	   // kernel calls
#include "fixed_point.h"
#include "psxpad.h" // defines for buttons.

#include "game.h" // for game functions

#include "dlist.h"

#define OTLEN 8
#define X_WIDTH 320
#define Y_HEIGHT 240

#define NULL 0

#define STARTING_ASTEROIDS 6
#define MIN_ASTEROID_SPLITTING_RADIUS 12
#define MAX_BULLETS 10

#define CNT1_COUNTS_PER_SECOND_NTSC 15625

// Define environment pairs and buffer counter
DISPENV disp[2];
DRAWENV draw[2];

int db;
int ot[2][OTLEN];

char pribuff[2][32768];
char *nextpri;

int tim_mode;
RECT tim_prect, tim_crect;
int tim_uoffs, tim_voffs;

u_char padbuff[2][34];

DList asteroids;
PolyObject_t player_ship;
PolyObject_t bullets[MAX_BULLETS];
int total_asteroids_destroyed = 0;

void display(void);
void die(char *message);
char *loadCdromfile(char *filename);

void die(char *message)
{

	while (1)
	{
		FntPrint(message);
		FntFlush(-1);
		display();
	}
}

void LoadTexture(u_long *tim, TIM_IMAGE *tparam)
{
	// Read TIM info
	OpenTIM(tim);
	ReadTIM(tparam);

	// Upload image to framebuffer
	LoadImage(tparam->prect, (u_long *)tparam->paddr);
	DrawSync(0);

	// Upload CLUT to framebuffer if present
	if (tparam->mode & 0x8)
	{
		LoadImage(tparam->crect, (u_long *)tparam->caddr);
		DrawSync(0);
	}
}

// a stupid name...
void LoadStuff(void)
{

	char *tim_bytes; // :D
	TIM_IMAGE zapotec;
	CdInit();

	tim_bytes = loadCdromfile("\\ZAPOTEC.TIM;1");
	if (tim_bytes == NULL)
	{
		die("FILE NOT LOADED");
	}

	LoadTexture((u_long *)tim_bytes, &zapotec);

	tim_prect = *zapotec.prect;
	tim_crect = *zapotec.crect;
	tim_mode = zapotec.mode;

	// Calulate u,v offset for TIMS that are not page aligned
	tim_uoffs = (tim_prect.x % 64) << (2 - (tim_mode & 0x03));
	tim_voffs = (tim_prect.y & 0xff);
	free(tim_bytes);
}

// CD loading function
char *loadCdromfile(char *filename)
{
	CdlFILE filePos;
	int numsecs;
	char *buff;

	buff = NULL;

	/* locate the file on the CD */
	if (CdSearchFile(&filePos, filename) == NULL)
	{
		/* print error message if file not found */
		printf("%s not found.", filename);
		die("FILE NOT FOUND");
	}
	else
	{
		/* calculate number of sectors to read for the file */
		numsecs = (filePos.size + 2047) / 2048;

		/* allocate buffer for the file (replace with malloc3() for PsyQ) */
		buff = (char *)malloc(2048 * numsecs);

		/* set read target to the file */
		CdControl(CdlSetloc, (u_char *)&filePos.pos, 0);

		/* start read operation */
		CdRead(numsecs, (u_long *)buff, CdlModeSpeed);

		/* wait until the read operation is complete */
		CdReadSync(0, 0);
	}

	return (buff);

} /* loadfile */

char *cdromLoadTexture(char *filename)
{
	int numsecs;
	CdlFILE filePos;
	char *buff;
	if (CdSearchFile(&filePos, filename) == NULL)
	{
		return NULL;
	}
	/* calculate number of sectors to read for the file */
	numsecs = (filePos.size + 2047) / 2048;

	/* allocate buffer */
	buff = (char *)malloc(2048 * numsecs);

	/* set read target (starting sector to read from) */
	CdControl(CdlSetloc, (u_char *)&filePos.pos, 0);

	/* Start read */
	CdRead(numsecs, (u_long *)buff, CdlModeSpeed);
	CdReadSync(0, 0); // if you want it to return sectors left, make first arg a 1, and keep it in a loop until returns 0.

	return buff;
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
	// Load font texture on upper right of VRAM
	FntLoad(960, 0);
	// Define a font window of 100 chars covering the whole screen
	FntOpen(0, 8, 320, 224, 0, 100);

	// load textures and possibly other stuff
	// LoadStuff();

	// set tpage of lone texture as initial tpage
	draw[0].tpage = getTPage(tim_mode & 0x3, 0, tim_prect.x, tim_prect.y);
	draw[1].tpage = getTPage(tim_mode & 0x3, 0, tim_prect.x, tim_prect.y);

	// Apply environments
	// PutDispEnv(&disp[0]);
	PutDrawEnv(&draw[!db]);

	InitPAD(padbuff[0], 34, padbuff[1], 34);
	padbuff[0][0] = padbuff[0][1] = 0xff; // init to ffff so no spurious input in the beginning.
	padbuff[1][0] = padbuff[1][1] = 0xff;

	StartPAD();
	srand(GetRCnt(RCntCNT1));
}

void textured_tile(int x, int y)
{
	SPRT *sprt;

	sprt = (SPRT *)nextpri;

	setSprt(sprt); // init it
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
	tile = (TILE *)nextpri;

	setTile(tile);
	setXY0(tile, x, y);
	setWH(tile, 64, 64);
	setRGB0(tile, 255, 255, 0);
	addPrim(ot[db], tile);
	nextpri += sizeof(TILE);
}

void split_asteroid(DListElmt *old_node)
{
	int i;
	PolyObject_t *new_asteroid;
	PolyObject_t *old_asteroid;
	void **data; // whatever

	old_asteroid = old_node->data;
	dlist_remove(&asteroids, old_node, data);

	if (old_asteroid->radius < MIN_ASTEROID_SPLITTING_RADIUS)
	{
		// we still have to free the old asteroid.

		free(old_asteroid);
		return;
	}

	for (i = 0; i < 2; i++)
	{
		new_asteroid = (PolyObject_t *)malloc(sizeof(PolyObject_t));
		GAME_make_asteroid(new_asteroid, old_asteroid->radius >> 1);
		new_asteroid->x = old_asteroid->x;
		new_asteroid->y = old_asteroid->y;
		new_asteroid->colour.vx = rand() % 256;
		new_asteroid->colour.vy = rand() % 256;
		new_asteroid->colour.vz = rand() % 256;
		new_asteroid->angle = rand() % 4096;
		new_asteroid->vel_x = (ccos(new_asteroid->angle) * (rand() % 5)) >> 1;
		new_asteroid->vel_y = (csin(new_asteroid->angle) * (rand() % 5)) >> 1;

		// insert it at the top of the list.
		dlist_ins_next(&asteroids, asteroids.head, new_asteroid);
	}
	// now we can free the old asteroid.
	free(old_asteroid);
	return;
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
	DrawOTag(ot[db] + OTLEN - 1);

	// Flip buffer counter
	db = !db;
	nextpri = pribuff[db];
}

void restart()
{
	int i;
	int angle_from_player;
	PolyObject_t *asteroid;
	player_ship.x = FIXED_POINT_ONE * 160;
	player_ship.y = FIXED_POINT_ONE * 160;
	player_ship.angle = 0;
	player_ship.vel_x = 0;
	player_ship.vel_y = 0;
	player_ship.numTris = 3;

	// destroy the old asteroids list.
	dlist_destroy(&asteroids);

	// now init the new list:
	dlist_init(&asteroids, free);

	for (i = 0; i < STARTING_ASTEROIDS; i++)
	{
		asteroid = (PolyObject_t *)malloc(sizeof(PolyObject_t));
		angle_from_player = rand() % 4096;
		GAME_make_asteroid(asteroid, 50);
		asteroid->x = player_ship.x + (ccos(angle_from_player) * ((rand() % 50) + 100));
		asteroid->y = player_ship.y - (csin(angle_from_player) * ((rand() % 50) + 100));
		asteroid->colour.vx = rand() % 256;
		asteroid->colour.vy = rand() % 256;
		asteroid->colour.vz = rand() % 256;
		asteroid->angle = rand() % 4096;
		asteroid->vel_x = (ccos(asteroid->angle) * ((rand() % 3) + 1)) >> 1;
		asteroid->vel_y = (csin(asteroid->angle) * ((rand() % 3) + 1)) >> 1;

		dlist_ins_next(&asteroids, asteroids.tail, asteroid);
	}
}

int doStartScreen()
{
	PADTYPE *pad;
	FntPrint("WELCOME TO BLASTEROIDS! Press Start!\n");

	FntFlush(-1);
	display();
	while (1)
	{
		pad = (PADTYPE *)padbuff[0];
		if (pad->stat == 0)
		{
			// only parse when using digital pad, dual-analog and dual-shock.
			if (pad->type == PAD_TYPE_DIGITAL ||
				pad->type == PAD_TYPE_ANALOG ||
				pad->type == PAD_TYPE_DUAL)
			{
				if (!(pad->btn & PAD_START))
				{

					// do game init
					total_asteroids_destroyed = 0;
					restart();
					// then
					return 1; // go to gameplay.
				}
			}
		}
	}
}
int doWin()
{
	PADTYPE *pad;
	FntPrint("Nice job! Press Start to continue.\n");

	FntFlush(-1);
	display();
	while (1)
	{
		pad = (PADTYPE *)padbuff[0];
		if (pad->stat == 0)
		{
			// only parse when using digital pad, dual-analog and dual-shock.
			if (pad->type == PAD_TYPE_DIGITAL ||
				pad->type == PAD_TYPE_ANALOG ||
				pad->type == PAD_TYPE_DUAL)
			{
				if (!(pad->btn & PAD_START))
				{

					// do game init

					restart();
					// then
					return 1; // go to gameplay.
				}
			}
		}
	}
}
int doDeath()
{
	PADTYPE *pad;
	FntPrint("You Are DEAD! Final Score: %d Try Again?\n", total_asteroids_destroyed);

	FntFlush(-1);
	display();
	while (1)
	{
		pad = (PADTYPE *)padbuff[0];
		if (pad->stat == 0)
		{
			// only parse when using digital pad, dual-analog and dual-shock.
			if (pad->type == PAD_TYPE_DIGITAL ||
				pad->type == PAD_TYPE_ANALOG ||
				pad->type == PAD_TYPE_DUAL)
			{
				if (!(pad->btn & PAD_START))
				{

					// do game init
					total_asteroids_destroyed = 0;
					restart();
					// then
					return 1; // go to gameplay.
				}
			}
		}
	}
}

void doPause()
{
	int startButtonHeld = 1;
	PADTYPE *pad;

	FntPrint("Paused. Press Start to continue.\n");

	FntFlush(-1);
	display();
	while (1)
	{
		pad = (PADTYPE *)padbuff[0];
		if (pad->stat == 0)
		{
			// only parse when using digital pad, dual-analog and dual-shock.
			if (pad->type == PAD_TYPE_DIGITAL ||
				pad->type == PAD_TYPE_ANALOG ||
				pad->type == PAD_TYPE_DUAL)
			{
				if (!(pad->btn & PAD_START) && startButtonHeld == 0)
				{
					return; // go to gameplay.
				}
				if ((pad->btn & PAD_START))
				{ // make sure the player has a chance to let go of the button.
					startButtonHeld = 0;
				}
			}
		}
	}
}

int doGamePlay()
{
	int i, j;
	static int count = 0;
	static int lastcount = 0;
	static int frames = 0;
	static int fps = 0;
	DListElmt *asteroid_iter;
	PolyObject_t *asteroid;
	PADTYPE *pad;
	static int next_bullet = 0;
	static int startButtonHeld = 1; // assume the player pressed start to begin the game.

	pad = (PADTYPE *)padbuff[0];
	if (pad->stat == 0)
	{
		// only parse when using digital pad, dual-analog and dual-shock.
		if (pad->type == PAD_TYPE_DIGITAL ||
			pad->type == PAD_TYPE_ANALOG ||
			pad->type == PAD_TYPE_DUAL)
		{
			if ((pad->btn & PAD_START))
			{ // make sure the player has a chance to let go of the button.
				startButtonHeld = 0;
			}
			if ((!(pad->btn & PAD_START)) && (startButtonHeld == 0))
			{
				startButtonHeld = 1;
				doPause();
			}

			if (!(pad->btn & PAD_R1))
			{
				if (bullets[next_bullet].numTris == 0)
				{
					GAME_make_bullet(&bullets[next_bullet], &player_ship);
				}
				next_bullet = (next_bullet + 1) % MAX_BULLETS;
			}
			if (!(pad->btn & PAD_UP))
			{
				player_ship.vel_x += csin(player_ship.angle) >> 3;
				player_ship.vel_y -= ccos(player_ship.angle) >> 3;
			}
			else if (!(pad->btn & PAD_DOWN))
			{
				player_ship.vel_x -= csin(player_ship.angle) >> 3;
				player_ship.vel_y += ccos(player_ship.angle) >> 3;
			}
			if (!(pad->btn & PAD_CROSS))
			{
				player_ship.vel_x += csin(player_ship.angle) >> 3;
				player_ship.vel_y -= ccos(player_ship.angle) >> 3;
			}
			else if (!(pad->btn & PAD_TRIANGLE))
			{
				if (player_ship.vel_x > BRAKE_POWER)
					player_ship.vel_x -= BRAKE_POWER;
				else if (player_ship.vel_x < -BRAKE_POWER)
					player_ship.vel_x += BRAKE_POWER;
				else
					player_ship.vel_x = 0;
				if (player_ship.vel_y > BRAKE_POWER)
					player_ship.vel_y -= BRAKE_POWER;
				else if (player_ship.vel_y < -BRAKE_POWER)
					player_ship.vel_y += BRAKE_POWER;
				else
					player_ship.vel_y = 0;
			}
			if (!(pad->btn & PAD_LEFT))
			{
				player_ship.angle -= 128;
			}
			else if (!(pad->btn & PAD_RIGHT))
			{
				player_ship.angle += 128;
			}
		}
	}

	ClearOTagR(ot[db], OTLEN);
	GAME_update_position(&player_ship, 0);
	nextpri = GAME_draw_triangle(&player_ship, ot[db], nextpri);
	for (i = 1; i < MAX_BULLETS; i++)
	{
		GAME_update_position(&bullets[i], 1);
		if (bullets[i].numTris > 0)
		{
			for (asteroid_iter = asteroids.head; asteroid_iter != NULL; asteroid_iter = asteroid_iter->next)
			{
				asteroid = (PolyObject_t *)asteroid_iter->data;
				if (asteroid->numTris == 0) // don't test dead asteroids
					continue;
				if (GAME_are_Objects_colliding(asteroid, &bullets[i]) == 1)
				{
					asteroid->numTris = 0;
					bullets[i].numTris = 0;
					total_asteroids_destroyed++;
					split_asteroid(asteroid_iter);
					return 1; // is it something to do with modifying the list?
				}
			}
			nextpri = GAME_draw_triangle(
				&bullets[i],
				ot[db],
				nextpri);
		}
	}
	for (asteroid_iter = asteroids.head; asteroid_iter != NULL; asteroid_iter = asteroid_iter->next)
	{
		asteroid = (PolyObject_t *)asteroid_iter->data;
		if (asteroid->numTris == 0)
			continue;
		GAME_update_position(asteroid, 0);
		nextpri = GAME_draw_asteroid(
			asteroid,
			ot[db],
			nextpri);
		if (GAME_are_Objects_colliding(asteroid, &player_ship) == 1)
		{
			startButtonHeld = 1; // they will press start at some point to continue...
			return 2;			 // death!
		}
	}
	// Looks like my slowdown is due to not finishing calculations before the next frame.
	count = GetRCnt(RCntCNT1) % (CNT1_COUNTS_PER_SECOND_NTSC >> 2);
	frames++;
	if (count < lastcount && frames != 0)
	{
		fps = frames << 2;
		frames = 0;
	}
	lastcount = count;
	FntPrint("Asteroids Left: %d Total destroyed: %d\n", asteroids.size, total_asteroids_destroyed);
	FntPrint("FPS %d ", fps);
	FntFlush(-1);

	display();

	if (asteroids.size == 0)
	{
		startButtonHeld = 1; // they will press start at some point to continue...
		return 3;			 // win!
	}

	return 1; // still in gameplay.
}

int main()
{
	int i, j;
	int mode = 1; // 0 start 1 play 2 dead

	SVECTOR player_colour = {255, 255, 0};
	GAME_make_ship(&player_ship, player_colour);

	// Initialize graphics and stuff
	init();

	GAME_init();

	restart();

	// Main loop
	while (1)
	{

		switch (mode)
		{
		case 0:
			mode = doStartScreen();
			break;
		case 1:
			mode = doGamePlay();
			break;
		case 2:
			mode = doDeath();
			break;
		case 3:
			mode = doWin();
			break;
		}
	}

	return 0;
}
