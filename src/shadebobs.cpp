//
// shadebobs.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define maxdegrees 256
#define divd 128
#define SIMULATION_STEP (1.0 / 60.0)

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

void DrawShadebob(int x, int y, int imagewidth, int imageheight, unsigned char* image, unsigned char *buffer = RETRO.framebuffer)
{
	float xstart = x - imagewidth / 2;
	float ystart = y - imageheight / 2;

	for (int yy = 0; yy < imageheight; yy++) {
		for (int xx = 0; xx < imagewidth; xx++) {
			int xpos = xx + xstart;
			int ypos = yy + ystart;
			if (xpos >= 0 && xpos < RETRO_WIDTH && ypos >= 0 && ypos < RETRO_HEIGHT) {
				buffer[ypos * RETRO_WIDTH + xpos] += image[yy * imagewidth + xx];
			}
		}
	}
}

void UpdateShadebobs(void)
{
	static float xadd1 = 60, xadd2 = 100, yadd1 = 55, yadd2 = 200;

	xadd1 += 200 * SIMULATION_STEP;
	xadd2 += 300 * SIMULATION_STEP;
	yadd1 += 300 * SIMULATION_STEP;
	yadd2 += 200 * SIMULATION_STEP;

	int x, y;

	x = sin(xadd1 * 2.0 * M_PI / maxdegrees) * divd / 2 + sin(xadd2 * 2.0 * M_PI / maxdegrees) * divd / 2;
	y = sin(yadd1 * 2.0 * M_PI / maxdegrees) * divd / 3 + sin(yadd2 * 2.0 * M_PI / maxdegrees) * divd / 3;
	DrawShadebob(160 + x, 120 + y, 16, 16, Image);

	x = sin((xadd1 + 2) * 2.0 * M_PI / maxdegrees) * divd / 2 + sin((xadd2 + 3) * 2.0 * M_PI / maxdegrees) * divd / 2;
	y = sin((yadd1 + 2) * 2.0 * M_PI / maxdegrees) * divd / 3 + sin((yadd2 + 3) * 2.0 * M_PI / maxdegrees) * divd / 3;
	DrawShadebob(160 + x, 120 + y, 16, 16, Image);

	x = sin(xadd1 * 2.0 * M_PI / maxdegrees) * divd / 2 + sin((xadd2 + 3) * 2.0 * M_PI / maxdegrees) * divd / 2;
	y = sin(yadd1 * 2.0 * M_PI / maxdegrees) * divd / 3 + sin((yadd2 + 2) * 2.0 * M_PI / maxdegrees) * divd / 3;
	DrawShadebob(160 + x, 120 + y, 16, 16, Image);

	x = sin((xadd1 + 2) * 2.0 * M_PI / maxdegrees) * divd / 2 + sin(xadd2 * 2.0 * M_PI / maxdegrees) * divd / 2;
	y = sin(yadd1 * 2.0 * M_PI / maxdegrees) * divd / 3 + sin((yadd2 + 2) * 2.0 * M_PI / maxdegrees) * divd / 3;
	DrawShadebob(160 + x, 120 + y, 16, 16, Image);
}

void DEMO_Render2(double deltatime)
{
	static double accumulator = 0;
	accumulator = MIN(accumulator + deltatime, SIMULATION_STEP * 15);
	while (accumulator >= SIMULATION_STEP) {
		UpdateShadebobs();
		accumulator -= SIMULATION_STEP;
	}
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette
	unsigned char r = 0, g = 0, b = 0;
	for(int i = 0; i < 64; i++) {
		RETRO_SetColor(i, r, 0, 0);
		r += 4;
	}
	for(int i = 64; i < 128; i++) {
		RETRO_SetColor(i, 255, g, 0);
		g += 4;
	}
	for(int i = 128; i < 256; i++) {
		RETRO_SetColor(i, 255, 255, b);
		b += 2;
	}
}
