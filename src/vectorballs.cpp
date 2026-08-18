//
// Vector balls
//
// A ring of balls, rippling in depth and tumbling about all three axes. Ball
// i sits at angle a = i turns / BALLS on a circle of BALL_RADIUS, pushed out
// of the plane by
//
//   z = BALL_WAVE sin(phase + WAVES a)
//
// WAVES is a whole number, so the wave closes around the ring: ball 0 and
// ball BALLS - 1 are neighbours on the same wave rather than either side of a
// step. The ring is rebuilt every frame, then rotated and projected as loose
// vertices, the way stars3.cpp carries its box of stars.
//
// A ball is a sprite, not a mesh: RETRO_CreateBallMap draws the lit sphere
// once, along with the hemisphere's depth, and RETRO_DrawDepthSprite scales
// it to BALL_SIZE * EYE * q, so it grows and shrinks with the same divide
// that places it. That drawer writes every pixel at the depth of the surface
// under it rather than the centre's, so two balls resolve on the curve where
// the spheres meet and there is no order to keep: the balls are drawn in the
// order they were laid out.
//
// Depth is also a color. The palette holds LEVELS ramps of SHADES, each the
// same black to hue to white but dimmer than the one before, and a sprite is
// built per ramp, so choosing the sprite by rotated depth dims the whole ball
// without touching its shading. Entry 0 is left as the transparent one.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropoly.h"
#include "lib/retrocolor.h"

#define BALLS 32
#define BALL_MAP 48 // the lit sphere is drawn once at this size and scaled from it
#define BALL_SIZE 27 // pixels across at the depth the ring's centre stands
#define BALL_RADIUS 72 // the ring, in pixels, since the projection adds no scale
#define BALL_WAVE 34 // pixels either way the ripple carries a ball out of the plane
#define BALL_WAVES 3 // whole waves around the ring
#define BALL_LEVELS 8 // depth ramps, and sprites, one per step of dimming
#define BALL_SHADES 30 // palette entries a ramp spends on one ball
#define BALL_DIM 0.35 // how much of its color the furthest ball keeps
#define BALL_DEPTH 110 // rotated depth the ramps cover, either side of the middle
#define BALL_HUE RETRO_CYAN
#define BALL_PROJECTION 1.0 // the ring is built in pixels, so the projection adds no scale
#define BALL_SPEEDX 0.7 // radians a second, about each axis
#define BALL_SPEEDY 1.1
#define BALL_SPEEDZ 0.4
#define BALL_WAVESPEED 60 // table units a second

Vertex Balls[BALLS];
unsigned char BallMap[BALL_LEVELS][BALL_MAP * BALL_MAP];
float BallDepth[BALL_MAP * BALL_MAP]; // the front hemisphere, the same for every ramp

void DEMO_Render(double deltatime)
{
	// Calculate rotation
	static float ax, ay, az;
	ax = fmod(ax + deltatime * BALL_SPEEDX, 2 * M_PI);
	ay = fmod(ay + deltatime * BALL_SPEEDY, 2 * M_PI);
	az = fmod(az + deltatime * BALL_SPEEDZ, 2 * M_PI);

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * BALL_WAVESPEED, RETRO_SINCOS_ANGLE);

	// Lay the ring out, ripple it, and carry it to the screen
	for (int i = 0; i < BALLS; i++) {
		float a = (float)RETRO_SINCOS_ANGLE * i / BALLS;
		Balls[i].x = BALL_RADIUS * COS(a);
		Balls[i].y = BALL_RADIUS * SIN(a);
		Balls[i].z = BALL_WAVE * SIN(phase + BALL_WAVES * a);

		RETRO_RotateVertex(&Balls[i], ax, ay, az);
		RETRO_ProjectVertex(&Balls[i], BALL_PROJECTION);
	}

	// Draw them, every pixel at the depth of the sphere's surface under it
	RETRO_ClearDepthBuffer();

	for (int i = 0; i < BALLS; i++) {
		Vertex *ball = &Balls[i];
		float size = BALL_SIZE * RETRO_PROJECTION_EYE * ball->q;
		int level = CLAMP((BALL_DEPTH + ball->rz) * BALL_LEVELS / (2 * BALL_DEPTH), 0, BALL_LEVELS - 1);

		RETRO_DrawDepthSprite(ball->sx, ball->sy, ball->q, size, BALL_SIZE / 2.0f, BallMap[level], BallDepth, BALL_MAP);
	}
}

void DEMO_Initialize(void)
{
	// Init palette. One ramp per depth, black to hue to white, each dimmer than
	// the one in front of it. Entry 0 is the background and the sprites' alpha
	RETRO_SetColor(0, RETRO_BLACK);

	RETRO_Palette hue = BALL_HUE;
	for (int k = 0; k < BALL_LEVELS; k++) {
		float dim = 1.0f - (1.0f - BALL_DIM) * k / (BALL_LEVELS - 1);
		RETRO_Palette lit = RETRO_Palette{ (unsigned char)(hue.r * dim), (unsigned char)(hue.g * dim), (unsigned char)(hue.b * dim) };
		RETRO_Palette top = RETRO_Palette{ (unsigned char)(255 * dim), (unsigned char)(255 * dim), (unsigned char)(255 * dim) };

		int ramp = 1 + k * BALL_SHADES;
		int middle = ramp + (BALL_SHADES * 2) / 3;

		RETRO_CreateGradientPalette(ramp, middle, RETRO_BLACK, lit);
		RETRO_CreateGradientPalette(middle, ramp + BALL_SHADES, lit, top);
	}

	// Init ball sprites, one per ramp, so a ball is dimmed by choosing its map
	for (int k = 0; k < BALL_LEVELS; k++) {
		RETRO_CreateBallMap(BallMap[k], BallDepth, BALL_MAP, 1 + k * BALL_SHADES, BALL_SHADES);
	}
}
