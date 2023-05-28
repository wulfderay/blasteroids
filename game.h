#ifndef __GAME_H
#define __GAME_H

#include <sys/types.h> // This provides typedefs needed by libgte.h and libgpu.h
#include <libgte.h>
#define BRAKE_POWER 512
#define ASTEROID_EDGES 13
#define MAX_ASTEROIDS 30
#define MAX_POINTS_PER_OBJECT (ASTEROID_EDGES * 3)

typedef struct
{
	SVECTOR tris[MAX_POINTS_PER_OBJECT];
	int numTris;
	SVECTOR colour;
	int x;
	int y;
	int angle;
	int vel_x;
	int vel_y;
	int radius;
} PolyObject_t;

PolyObject_t *GAME_make_asteroid(PolyObject_t *asteroid, int max_radius);
PolyObject_t *GAME_make_ship(PolyObject_t *ship, SVECTOR colour);

char *GAME_draw_triangle(PolyObject_t *ship, int *orderingTable, char *nextPrim);
char *GAME_draw_asteroid(PolyObject_t *asteroid, int *orderingTable, char *nextPrim);

PolyObject_t *GAME_update_position(PolyObject_t *drawable, int cull_when_offscreen);

int GAME_are_Objects_colliding(PolyObject_t *drawable1, PolyObject_t *drawable2);

void GAME_init();

#endif
