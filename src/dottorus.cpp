//
// dottorus.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrogfx.h"

#define NUM_TORUS 5
#define K 15
#define POINTS 100
#define BLUR 2
#define NUM_COLORS 163
#define SCALE 1
#define SINE_VALUES 720

// Big 5x5 torus block
unsigned char Form[NUM_TORUS][NUM_TORUS] = {
	{K * 0, K * 3, K * 3, K * 3, K * 0},
	{K * 2, K * 4, K * 4, K * 4, K * 2},
	{K * 3, K * 4, K * 5, K * 4, K * 3},
	{K * 2, K * 4, K * 4, K * 4, K * 2},
	{K * 0, K * 3, K * 3, K * 3, K * 0}
};

Point3Df Points[POINTS] = {
	{19, 0, -13},
	{8, 0, -21},
	{-7, 0, -22},
	{-18, 0, -13},
	{-22, 0, 1},
	{-18, 0, 14},
	{-7, 0, 22},
	{7, 0, 22},
	{19, 0, 14},
	{23, 0, 1},
	{4, 47, -13},
	{-6, 41, -21},
	{-17, 32, -22},
	{-26, 26, -13},
	{-30, 23, 0},
	{-26, 26, 14},
	{-17, 32, 22},
	{-6, 41, 22},
	{3, 47, 14},
	{7, 50, 0},
	{-37, 77, -13},
	{-40, 66, -21},
	{-45, 53, -22},
	{-48, 42, -13},
	{-50, 37, 0},
	{-48, 42, 14},
	{-45, 52, 22},
	{-40, 66, 22},
	{-37, 77, 14},
	{-35, 81, 0},
	{-87, 77, -13},
	{-83, 66, -21},
	{-79, 53, -22},
	{-75, 42, -13},
	{-74, 37, 0},
	{-75, 42, 14},
	{-79, 52, 22},
	{-83, 66, 22},
	{-87, 77, 14},
	{-88, 81, 0},
	{-127, 47, -13},
	{-118, 41, -21},
	{-106, 32, -22},
	{-97, 26, -13},
	{-94, 23, 0},
	{-97, 26, 14},
	{-106, 32, 22},
	{-118, 41, 22},
	{-127, 47, 14},
	{-130, 50, 0},
	{-142, 0, -13},
	{-131, 0, -21},
	{-117, 0, -22},
	{-105, 0, -13},
	{-101, 0, 0},
	{-105, 0, 14},
	{-117, 0, 22},
	{-131, 0, 22},
	{-142, 0, 14},
	{-147, 0, 0},
	{-127, -47, -13},
	{-118, -41, -21},
	{-106, -32, -22},
	{-97, -26, -13},
	{-94, -23, 0},
	{-97, -26, 14},
	{-106, -32, 22},
	{-118, -41, 22},
	{-127, -47, 14},
	{-130, -50, 0},
	{-87, -77, -13},
	{-83, -66, -21},
	{-79, -53, -22},
	{-75, -42, -13},
	{-74, -37, 0},
	{-75, -42, 14},
	{-79, -52, 22},
	{-83, -66, 22},
	{-87, -77, 14},
	{-88, -81, 0},
	{-37, -77, -13},
	{-40, -66, -21},
	{-45, -53, -22},
	{-48, -42, -13},
	{-50, -37, 0},
	{-48, -42, 14},
	{-45, -52, 22},
	{-40, -66, 22},
	{-37, -77, 14},
	{-35, -81, 0},
	{4, -47, -13},
	{-6, -41, -21},
	{-17, -32, -22},
	{-26, -26, -13},
	{-30, -23, 0},
	{-26, -26, 14},
	{-17, -32, 22},
	{-6, -41, 22},
	{3, -47, 14},
	{7, -50, 0}
};

float SinTable[SINE_VALUES];
float CosTable[SINE_VALUES];

void DEMO_Render2(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 200;
	int frame = frame_counter;

	// Draw blob
	for (int b = 0; b < BLUR; b++) {
		for (int p = 0; p < POINTS; p++) {
			Point3Df rotated = RETRO_RotatePoint(Points[p], CosTable[(frame + b) % SINE_VALUES], SinTable[(frame + b) % SINE_VALUES]);
			Point2D projected = RETRO_ProjectPoint(rotated, SCALE);

			for (int y = 0; y < NUM_TORUS; y++) {
				for (int x = 0; x < NUM_TORUS; x++) {
					unsigned char color = RETRO_GetPixel(projected.x + x, projected.y + y) + Form[x][y];

					if (color > NUM_COLORS) {
						color = NUM_COLORS;
					}

					RETRO_PutPixel(projected.x + x, projected.y + y, color);
				}
			}
		}
	}

	RETRO_Blur(RETRO_BLUR_4, 3);
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init sine table
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * 2 * M_PI / SINE_VALUES) * 0.7;
		CosTable[i] = cos(i * 2 * M_PI / SINE_VALUES);
	}

	// Init palette
	for (int i = 0; i < NUM_COLORS; i++) {
		unsigned char r = NUM_COLORS * exp(2 * log((double) i / (NUM_COLORS - 10)));
		unsigned char g = NUM_COLORS * exp(7 * log((double) i / (NUM_COLORS - 10)));
		unsigned char b = NUM_COLORS * exp(7 * log((double) i / (NUM_COLORS - 10)));
		RETRO_SetColor(i, r, g, b);
	}
}
