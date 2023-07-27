//
// metaballs.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define NUM_BALLS 4
#define SINE_VALUES 360

struct MetaBall {
	float x, y, r;
} Balls[NUM_BALLS];

float SinTable[SINE_VALUES];
float CosTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 100;
	int frame = (int)frame_counter % SINE_VALUES;

	// Move balls
	Balls[0].r = 1000;
	Balls[0].x = CosTable[frame] * -100 + (RETRO_WIDTH / 2);
	Balls[0].y = SinTable[frame] * -10 + (RETRO_HEIGHT / 2);
	Balls[1].r = 4000;
	Balls[1].x = CosTable[frame] * 10 + (RETRO_WIDTH / 2);
	Balls[1].y = SinTable[frame] * 60 + (RETRO_HEIGHT / 2);
	Balls[2].r = 7000;
	Balls[2].x = CosTable[frame] * -130 + (RETRO_WIDTH / 2);
	Balls[2].y = SinTable[frame] * -80 + (RETRO_HEIGHT / 2);
	Balls[3].r = 10000;
	Balls[3].x = CosTable[frame] * -80 + (RETRO_WIDTH / 2);
	Balls[3].y = SinTable[frame] * 70 + (RETRO_HEIGHT / 2);

	// Draw balls
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float color = 0;
			for (int i = 0; i < NUM_BALLS; i++) {
				color += Balls[i].r / float ((x - Balls[i].x) * (x - Balls[i].x) + (y - Balls[i].y) * (y - Balls[i].y));
			}
			color = CLAMP256(20 * color);

			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize()
{
	// Init tables
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i / 180.0 * M_PI);
		CosTable[i] = cos(i / 180.0 * M_PI);
	}

	// Init palette
	int light = 350;
	int reflect = 130;
	int ambient = 0;
	for (int i = 0; i < RETRO_COLORS; i++) {
		double intensity = cos((255 - i) / 512.0 * M_PI);
		int r = CLAMP256(63 * ambient / 255.0 + 63 * intensity + pow(intensity, reflect) * light);
		int g = CLAMP256(72 * ambient / 255.0 + 72 * intensity + pow(intensity, reflect) * light);
		int b = CLAMP256(128 * ambient / 255.0 + 128 * intensity + pow(intensity, reflect) * light);
		RETRO_SetColor(i, r, g, b);
	}
}
