//
// Shadebobs
//
// 16×16 binary discs stamped additively onto a framebuffer that is never
// cleared. Four bobs follow Lissajous sums of two sines (period 256),
//
//   x = W/2 + (A/2) (sin xphase1 + sin xphase2)
//   y = H/2 + (A/3) (sin yphase1 + sin yphase2)
//
// offset from each other by a couple of table steps so they travel as a
// cluster. Adding wraps at 256, so a pixel under the path walks the
// palette once every 256 stamps. The palette is a heat ramp,
// black-red-yellow-white. xphase1, xphase2, yphase1, yphase2 live on 256.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define BOB_SIZE 16
#define BOB_AMP 128
#define BOB_SPEED1 200 // table units per second
#define BOB_SPEED2 300

unsigned char Image[] = {
	 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
	 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
	 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
	 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 };

void DrawShadebob(double xphase1, double xphase2, double yphase1, double yphase2)
{
	int x = RETRO_WIDTH / 2 + SIN(xphase1) * BOB_AMP / 2 + SIN(xphase2) * BOB_AMP / 2;
	int y = RETRO_HEIGHT / 2 + SIN(yphase1) * BOB_AMP / 3 + SIN(yphase2) * BOB_AMP / 3;
	int xstart = x - BOB_SIZE / 2;
	int ystart = y - BOB_SIZE / 2;

	for (int yy = 0; yy < BOB_SIZE; yy++) {
		for (int xx = 0; xx < BOB_SIZE; xx++) {
			int xpos = xx + xstart;
			int ypos = yy + ystart;
			if (xpos >= 0 && xpos < RETRO_WIDTH && ypos >= 0 && ypos < RETRO_HEIGHT) {
				RETRO.framebuffer[ypos * RETRO_WIDTH + xpos] += Image[yy * BOB_SIZE + xx];
			}
		}
	}
}

//
// Advance the bobs in fixed steps. DrawShadebob adds into a framebuffer that is never
// cleared and wraps at 256 rather than saturating, so the color banding is a function
// of how many bobs have been drawn. At 60 Hz a pixel under the path cycles the palette
// every 256 draws.
//
void DEMO_FixedUpdate(double timestep)
{
	// Calculate phase
	static double xphase1 = 60, xphase2 = 100, yphase1 = 55, yphase2 = 200;

	xphase1 = fmod(xphase1 + BOB_SPEED1 * timestep, RETRO_SINCOS_ANGLE);
	xphase2 = fmod(xphase2 + BOB_SPEED2 * timestep, RETRO_SINCOS_ANGLE);
	yphase1 = fmod(yphase1 + BOB_SPEED2 * timestep, RETRO_SINCOS_ANGLE);
	yphase2 = fmod(yphase2 + BOB_SPEED1 * timestep, RETRO_SINCOS_ANGLE);

	// Draw bobs
	DrawShadebob(xphase1, xphase2, yphase1, yphase2);
	DrawShadebob(xphase1 + 2, xphase2 + 3, yphase1 + 2, yphase2 + 3);
	DrawShadebob(xphase1, xphase2 + 3, yphase1, yphase2 + 2);
	DrawShadebob(xphase1 + 2, xphase2, yphase1, yphase2 + 2);
}

void DEMO_Initialize(void)
{
	// Init palette. The bobs heat up from red through yellow into white as
	// they pile on top of each other
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(64, 128, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
