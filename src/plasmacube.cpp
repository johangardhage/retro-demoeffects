//
// Plasma cube
//
// The same product of two travelling-wave sums as plasma.cpp, written
// into a 256² texture and mapped onto a cube. The table is sine rather
// than cosine (a 90° phase), one turn in degrees, indices WRAP360.
// t lives in [0, 720) so the integer t/2 term covers a full period.
//
//   X(x, t) = 75 + sin(2x + t/2) + sin(x + 2t) + 2 sin(x/2 + t)
//   Y(y, t) = 75 + 2 sin(y + 2t) + sin(2y + t/2) + 2 sin(y + t)
//   color   = (X Y) mod 252
//
// The field is regenerated every frame so the cube carries a live
// plasma. The cube turn is Rz Ry Rx; the Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define PLASMA_FRAMES 720
#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define ROTATION_SPEED 2 // radians a second, about each axis

float SinTable[RETRO_DEGREES_PER_TURN];
unsigned char image[TEXTURE_WIDTH * TEXTURE_HEIGHT];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * 80, PLASMA_FRAMES);
	int iphase = (int)phase;

	// Generate plasma
	for (int y = 0; y < TEXTURE_HEIGHT; y++) {
		float yc = 75 + SinTable[WRAP360(y + iphase * 2)] * 2 + SinTable[WRAP360(y * 2 + iphase / 2)] + SinTable[WRAP360(y + iphase)] * 2;

		for (int x = 0; x < TEXTURE_WIDTH; x++) {
			float xc = 75 + SinTable[WRAP360(x * 2 + iphase / 2)] + SinTable[WRAP360(x + iphase * 2)] + SinTable[WRAP360(x / 2 + iphase)] * 2;

			// Wrap into the 252-entry palette cycle
			unsigned char color = (int)(yc * xc) % 252;
			image[y * TEXTURE_WIDTH + x] = color;
		}
	}

	// Rotate cube
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_TEXTURE);
}

void DEMO_Initialize(void)
{
	// Init palette. The 252 cycling colors ramp one channel at a time, from
	// black through red, yellow, white, cyan and blue, and back to black
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_BLACK);
	RETRO_CreateGradientPalette(0, 42, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(42, 84, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(84, 126, RETRO_YELLOW, RETRO_WHITE);
	RETRO_CreateGradientPalette(126, 168, RETRO_WHITE, RETRO_CYAN);
	RETRO_CreateGradientPalette(168, 210, RETRO_CYAN, RETRO_BLUE);
	RETRO_CreateGradientPalette(210, 252, RETRO_BLUE, RETRO_BLACK);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->texmap = image;

	// Init tables
	for (int i = 0; i < RETRO_DEGREES_PER_TURN; i++) {
		SinTable[i] = sin(i * DEG2RAD);
	}
}
