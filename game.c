#include "game.h"
#include "fixed_point.h"
#include <sys/types.h> // This provides typedefs needed by libgte.h and libgpu.h
#include <stdio.h>	   // for rand
#include <libetc.h>	   // Includes some functions that controls the display
#include <libgte.h>	   // GTE header, not really used but libgpu.h depends on it
#include <libgpu.h>	   // GPU library header
#include <libcd.h>	   // cdrom utils
#include "psxpad.h"	   // defines for buttons.

SVECTOR player_tri[] =
	{
		{0, -10, 0},
		{5, 10, 0},
		{-5, 10, 0},
};
SVECTOR bullet_tri[] =
	{
		{0, -4, 0},
		{2, 4, 0},
		{-2, 4, 0},
};

PolyObject_t *GAME_make_ship(PolyObject_t *ship, SVECTOR colour)
{
	int i;
	ship->numTris = 3;
	for (i = 0; i < 3; i++)
	{
		ship->tris[i].vx = player_tri[i].vx;
		ship->tris[i].vy = player_tri[i].vy;
		ship->tris[i].vz = player_tri[i].vz;
	}
	ship->colour.vx = colour.vx;
	ship->colour.vy = colour.vy;
	ship->colour.vz = colour.vz;
	return ship;
}

char *GAME_draw_triangle(PolyObject_t *ship, int *orderingTable, char *nextPrim)
{

	SVECTOR v[3];
	POLY_F3 *tri;
	int i;
	if (ship->numTris == 0) // don't draw if dead.
		return nextPrim;
	// Rotate the triangle coordinates based on the player's angle
	// as well as apply the position
	for (i = 0; i < 3; i++)
	{
		v[i].vx = (((ship->tris[i].vx * ccos(ship->angle)) - (ship->tris[i].vy * csin(ship->angle))) >> 12) + (ship->x >> 12);
		v[i].vy = (((ship->tris[i].vy * ccos(ship->angle)) + (ship->tris[i].vx * csin(ship->angle))) >> 12) + (ship->y >> 12);
	}

	// Sort the player triangle
	tri = (POLY_F3 *)nextPrim;
	setPolyF3(tri);
	setRGB0(tri, ship->colour.vx, ship->colour.vy, ship->colour.vz);
	setXY3(tri,
		   v[0].vx, v[0].vy,
		   v[1].vx, v[1].vy,
		   v[2].vx, v[2].vy);
	addPrim(orderingTable, tri);
	nextPrim += sizeof(POLY_F3);
	return nextPrim;
}

char *GAME_draw_asteroid(PolyObject_t *asteroid, int *orderingTable, char *nextPrim)
{
	SVECTOR v[3];
	POLY_F3 *tri;
	int i;
	int first, second;
	if (asteroid->numTris == 0) // don't draw if dead.
		return nextPrim;
	for (i = 0; i < ASTEROID_EDGES; i++)
	{
		if (i == ASTEROID_EDGES - 1)
		{
			first = 0;
			second = i;
		}
		else
		{

			first = i;
			second = i + 1;
		}
		v[0].vx = (((asteroid->tris[first].vx * ccos(asteroid->angle)) - (asteroid->tris[first].vy * csin(asteroid->angle))) >> 12) + (asteroid->x >> 12);
		v[0].vy = (((asteroid->tris[first].vy * ccos(asteroid->angle)) + (asteroid->tris[first].vx * csin(asteroid->angle))) >> 12) + (asteroid->y >> 12);

		v[1].vx = (((asteroid->tris[second].vx * ccos(asteroid->angle)) - (asteroid->tris[second].vy * csin(asteroid->angle))) >> 12) + (asteroid->x >> 12);
		v[1].vy = (((asteroid->tris[second].vy * ccos(asteroid->angle)) + (asteroid->tris[second].vx * csin(asteroid->angle))) >> 12) + (asteroid->y >> 12);

		v[2].vx = (asteroid->x >> 12); // aka 0,0 in local space
		v[2].vy = (asteroid->y >> 12);

		// Sort the player triangle
		tri = (POLY_F3 *)nextPrim;
		setPolyF3(tri);
		setRGB0(tri, asteroid->colour.vx, asteroid->colour.vy, asteroid->colour.vz);
		setXY3(tri,
			   v[0].vx, v[0].vy,
			   v[1].vx, v[1].vy,
			   v[2].vx, v[2].vy);
		addPrim(orderingTable, tri);
		nextPrim += sizeof(POLY_F3);
	}

	// I don't know why, but the first triangle never shows up... This is a dirty hack to draw it again after... :/

	v[0].vx = (((asteroid->tris[0].vx * ccos(asteroid->angle)) - (asteroid->tris[0].vy * csin(asteroid->angle))) >> 12) + (asteroid->x >> 12);
	v[0].vy = (((asteroid->tris[0].vy * ccos(asteroid->angle)) + (asteroid->tris[0].vx * csin(asteroid->angle))) >> 12) + (asteroid->y >> 12);

	v[1].vx = (((asteroid->tris[1].vx * ccos(asteroid->angle)) - (asteroid->tris[1].vy * csin(asteroid->angle))) >> 12) + (asteroid->x >> 12);
	v[1].vy = (((asteroid->tris[1].vy * ccos(asteroid->angle)) + (asteroid->tris[1].vx * csin(asteroid->angle))) >> 12) + (asteroid->y >> 12);

	v[2].vx = (asteroid->x >> 12); // aka 0,0 in local space
	v[2].vy = (asteroid->y >> 12);

	// Sort the player triangle
	tri = (POLY_F3 *)nextPrim;
	setPolyF3(tri);
	setRGB0(tri, asteroid->colour.vx, asteroid->colour.vy, asteroid->colour.vz);
	setXY3(tri,
		   v[0].vx, v[0].vy,
		   v[1].vx, v[1].vy,
		   v[2].vx, v[2].vy);
	addPrim(orderingTable, tri);
	nextPrim += sizeof(POLY_F3);
	return nextPrim;
}

// TODO: Make this generate triangles rather than just the edges
PolyObject_t *GAME_make_asteroid(PolyObject_t *asteroid, int max_radius)
{
	int i;
	int angle = 0;
	u_int angle_step = 4096 / ASTEROID_EDGES;
	int x = 0;
	int y = 0;
	int min_radius = (max_radius * 2) / 3;
	int r;
	asteroid->numTris = ASTEROID_EDGES;
	asteroid->radius = (min_radius + max_radius) / 2;
	// generate asteroid
	for (i = 0; i < ASTEROID_EDGES; i++)
	{
		r = (rand() % (max_radius - min_radius)) + min_radius;
		angle = i * angle_step;
		x = r * ccos(angle);
		y = r * csin(angle);
		asteroid->tris[i].vx = (x >> 12);
		asteroid->tris[i].vy = (y >> 12);
		asteroid->tris[i].vz = 0;
	}
	return asteroid;
}

PolyObject_t *GAME_make_bullet(PolyObject_t *bullet, PolyObject_t *ship)
{
	int i;
	bullet->numTris = 3;
	for (i = 0; i < 3; i++)
	{
		bullet->tris[i].vx = bullet_tri[i].vx;
		bullet->tris[i].vy = bullet_tri[i].vy;
		bullet->tris[i].vz = bullet_tri[i].vz;
	}
	bullet->colour.vx = 200;
	bullet->colour.vy = 200;
	bullet->colour.vz = 200;
	bullet->x = ship->x;
	bullet->y = ship->y;
	bullet->angle = ship->angle;
	bullet->vel_x = (csin(bullet->angle)) * 5;
	bullet->vel_y = -(ccos(bullet->angle)) * 5;
	return bullet;
}

PolyObject_t *GAME_update_position(PolyObject_t *drawable, int cull_when_offscreen)
{
	if (drawable->numTris == 0) // this object has been culled. Don't update.
		return drawable;
	drawable->x += drawable->vel_x;
	drawable->y += drawable->vel_y;

	if ((drawable->x >> 12) < 0)
	{
		if (cull_when_offscreen)
			drawable->numTris = 0;
		drawable->x += (320 << 12);
	}
	if ((drawable->x >> 12) > 320)
	{
		if (cull_when_offscreen)
			drawable->numTris = 0;
		drawable->x -= (320 << 12);
	}
	if ((drawable->y >> 12) < 0)
	{
		if (cull_when_offscreen)
			drawable->numTris = 0;
		drawable->y += (240 << 12);
	}
	if ((drawable->y >> 12) > 240)
	{
		if (cull_when_offscreen)
			drawable->numTris = 0;
		drawable->y -= (240 << 12);
	}
}

int GAME_are_Objects_colliding(PolyObject_t *drawable1, PolyObject_t *drawable2)
{
	// calculate distance by pythagoras
	int csquared;
	int asquared;
	int bsquared;
	int radiusSquared = ((drawable1->radius) * (drawable1->radius)) + ((drawable2->radius) * (drawable2->radius));
	asquared = ((drawable1->x - drawable2->x) >> 12) * ((drawable1->x - drawable2->x) >> 12);
	bsquared = ((drawable1->y - drawable2->y) >> 12) * ((drawable1->y - drawable2->y) >> 12);
	csquared = asquared + bsquared;

	if (csquared < radiusSquared)
	{
		/*FntPrint("1x=%d (%d.%d) 1y=%d (%d.%d)\n", drawable1->x, (drawable1->x >> FIXED_POINT_BITS), (drawable1->x& FIXED_POINT_MANTISSA) ,drawable1->y, (drawable1->y >> FIXED_POINT_BITS), (drawable1->y& FIXED_POINT_MANTISSA) );
			FntPrint("2x=%d (%d.%d) 2y=%d (%d.%d)\n", drawable2->x, (drawable2->x >> FIXED_POINT_BITS), (drawable2->x& FIXED_POINT_MANTISSA), drawable2->y, (drawable2->y >> FIXED_POINT_BITS), (drawable2->y& FIXED_POINT_MANTISSA) );
		FntPrint("csquared=%d  minradius=%d \n", csquared,radiusSquared);*/
		return 1;
	}
	return 0;
}

// generate asteroids, etc.
void GAME_init()
{
}
