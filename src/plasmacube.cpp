//
// Plasma (texture) mapped cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define PLASMA_FRAMES 720
#define SINE_VALUES 1800
#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256

float SinTable[SINE_VALUES];
unsigned char image[TEXTURE_WIDTH * TEXTURE_HEIGHT];

void DEMO_Render(double deltatime)
{
	static float framecounter = 0;
	framecounter += deltatime * 80;
	int frame = WRAP(framecounter, PLASMA_FRAMES);

	// Generate plasma
	for (int y = 0; y < TEXTURE_HEIGHT; y++) {
		float yc = 75 + SinTable[y + frame * 2] * 2 + SinTable[y * 2 + frame / 2] + SinTable[y + frame] * 2;

		for (int x = 0; x < TEXTURE_WIDTH; x++) {
			float xc = 75 + SinTable[x * 2 + frame / 2] + SinTable[x + frame * 2] + SinTable[x / 2 + frame] * 2;

			// Wrap into the 252-entry palette cycle
			unsigned char color = (int)(yc * xc) % 252;
			image[y * TEXTURE_WIDTH + x] = color;
		}
	}

	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_TEXTURE);
}

void DEMO_Initialize(void)
{
	// Init palette. The 252 cycling colors ramp one channel at a time, up to
	// two thirds intensity, and back down again
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_BLACK);
	RETRO_CreateGradientPalette(0, 42, RETRO_BLACK, {168, 0, 0});
	RETRO_CreateGradientPalette(42, 84, {168, 0, 0}, {168, 168, 0});
	RETRO_CreateGradientPalette(84, 126, {168, 168, 0}, {168, 168, 168});
	RETRO_CreateGradientPalette(126, 168, {168, 168, 168}, {0, 168, 168});
	RETRO_CreateGradientPalette(168, 210, {0, 168, 168}, {0, 0, 168});
	RETRO_CreateGradientPalette(210, 252, {0, 0, 168}, RETRO_BLACK);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->texmap = image;

	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * M_PI / 180);
	}
}
