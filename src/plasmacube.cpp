//
// Plasma cube
//
// A plasma, the product of two traveling-wave sums, written into a 256²
// texture and mapped onto a cube. Sine rather than cosine (a 90° phase), with
// angles in degrees. t is the unrounded time times 80, so the flow is smooth;
// it lives on [0, 720) so the t/2 term covers a full period.
//
//   X(x, t) = 75 + sin(2x + t/2) + sin(x + 2t) + 2 sin(x/2 + t)
//   Y(y, t) = 75 + 2 sin(y + 2t) + sin(2y + t/2) + 2 sin(y + t)
//   color   = (X Y) mod 252
//
// The field is regenerated every frame so the cube carries a live
// plasma. X depends only on x and Y only on y, so each is worked out once
// per column and row rather than once per texel. The cube turn is Rz Ry Rx;
// the Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

constexpr int PLASMA_FRAMES = 720;
constexpr int TEXTURE_WIDTH = 256;
constexpr int TEXTURE_HEIGHT = 256;
constexpr int ROTATION_SPEED = 2; // radians a second, about each axis

static unsigned char Texture[TEXTURE_WIDTH * TEXTURE_HEIGHT];

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * 80, PLASMA_FRAMES);

	float columns[TEXTURE_WIDTH];
	for (int x = 0; x < TEXTURE_WIDTH; x++) {
		columns[x] = 75 + sin(radians(x * 2 + phase / 2)) + sin(radians(x + phase * 2)) + sin(radians(x / 2.0 + phase)) * 2;
	}

	// Generate plasma
	for (int y = 0; y < TEXTURE_HEIGHT; y++) {
		float yc = 75 + sin(radians(y + phase * 2)) * 2 + sin(radians(y * 2 + phase / 2)) + sin(radians(y + phase)) * 2;

		for (int x = 0; x < TEXTURE_WIDTH; x++) {
			// Wrap into the 252-entry palette cycle
			unsigned char color = (int)(yc * columns[x]) % 252;
			Texture[y * TEXTURE_WIDTH + x] = color;
		}
	}

	// Calculate rotation
	float ax = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time.total * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
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
	model->texmap = Texture;
}
