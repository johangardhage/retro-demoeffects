//
// Plasma (texture) mapped cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

#define PLASMA_FRAMES 720
#define SINE_VALUES 1800
#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256

float SinTable[SINE_VALUES];
unsigned char image[TEXTURE_WIDTH * TEXTURE_HEIGHT];

void DEMO_Render(double deltatime)
{
	static float framecounter = 0;
	framecounter += deltatime * 200;
	int frame = WRAP(framecounter, PLASMA_FRAMES);

	// Generate plasma
	for (int y = 0; y < TEXTURE_HEIGHT; y++) {
		float yc = 75 + SinTable[y + frame * 2] * 2 + SinTable[y * 2 + frame / 2] + SinTable[y + frame] * 2;

		for (int x = 0; x < TEXTURE_WIDTH; x++) {
			float xc = 75 + SinTable[x * 2 + frame / 2] + SinTable[x + frame * 2] + SinTable[x / 2 + frame] * 2;

			unsigned char color = yc * xc;
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
	unsigned char r = 0, g = 0, b = 0;
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, 0, 0, 0);
	}
	for (int i = 0; i < 42; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		r++;
	}
	for (int i = 42; i < 84; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		g++;
	}
	for (int i = 84; i < 126; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		b++;
	}
	for (int i = 126; i < 168; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		r--;
	}
	for (int i = 168; i < 210; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		g--;
	}
	for (int i = 210; i < 252; i++) {
		RETRO_SetColor(i, r * 4, g * 4, b * 4);
		b--;
	}

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->texmap = image;

	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * M_PI / 180);
	}
}
