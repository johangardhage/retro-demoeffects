//
// dotflag.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define FLAG_CURVE 125
#define FLAG_SIZE 4
#define FLAG_WIDTH 16
#define FLAG_HEIGHT 10
#define SINE_VALUES 255

unsigned char SwedishFlag[FLAG_HEIGHT][FLAG_WIDTH] = {
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

float SinTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 100;
	int frame = frame_counter;

	// Draw flag
	for (int y1 = 0; y1 < FLAG_HEIGHT; y1++) {
		for (int x1 = 0; x1 < FLAG_WIDTH; x1++) {
			unsigned char color = SwedishFlag[y1][x1];

			for (int y2 = 0; y2 < FLAG_SIZE; y2++) {
				for (int x2 = 0; x2 < FLAG_SIZE; x2++) {
					int xp = x1 * FLAG_SIZE + x2;
					int yp = y1 * FLAG_SIZE + y2;

					int xc = (RETRO_WIDTH / 2) - (FLAG_WIDTH * FLAG_SIZE * FLAG_SIZE) / 2;
					int yc = (RETRO_HEIGHT / 2) - (FLAG_HEIGHT * FLAG_SIZE * FLAG_SIZE) / 2;

					int x = xc + xp * FLAG_SIZE + SinTable[(frame + xp * FLAG_CURVE + yp * FLAG_CURVE + xp) % SINE_VALUES];
					int y = yc + yp * FLAG_SIZE + SinTable[(frame + xp * FLAG_CURVE + yp * FLAG_CURVE + yp) % SINE_VALUES];

					RETRO_PutPixel(x, y, color);
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init sin table
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * 4 * M_PI / SINE_VALUES) * 10;
	}

	// Init palette
	RETRO_SetColor(0, 0, 0, 0);
	RETRO_SetColor(1, 0, 0, 255);
	RETRO_SetColor(2, 255, 255, 0);
}
