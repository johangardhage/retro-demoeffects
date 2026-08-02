//
// dotmorph.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define POINTS 4096
#define SCALE 80

Vertex Sphere[POINTS];
Vertex Torus[POINTS];
Vertex Morph[POINTS];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 200;
	int frame = (int)frame_counter % 1536;

	// Calculate angle
	static double angle = 0.0;
	angle = fmod(angle + deltatime * 100 * M_PI / 180.0, 2 * M_PI);

	// Transform the torus into a sphere
	if (frame < 256) {
		int key = frame;
		for (int i = 0; i < POINTS; i++) {
			Morph[i].x = (key * Sphere[i].x + (256 - key) * Torus[i].x) / 256;
			Morph[i].y = (key * Sphere[i].y + (256 - key) * Torus[i].y) / 256;
			Morph[i].z = (key * Sphere[i].z + (256 - key) * Torus[i].z) / 256;
		}
	}

	// Transform the sphere into a torus
	if (frame > 768 && frame <= 1024) {
		int key = frame - 768;
		for (int i = 0; i < POINTS; i++) {
			Morph[i].x = (key * Torus[i].x + (256 - key) * Sphere[i].x) / 256;
			Morph[i].y = (key * Torus[i].y + (256 - key) * Sphere[i].y) / 256;
			Morph[i].z = (key * Torus[i].z + (256 - key) * Sphere[i].z) / 256;
		}
	}

	for (int i = 0; i < POINTS; i++) {
		RETRO_RotateVertex(&Morph[i], 0, angle, angle);
		RETRO_ProjectVertex(&Morph[i], SCALE);

		int x = Morph[i].sx;
		int y = Morph[i].sy;
		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			unsigned char color = ceil(SCALE * (Morph[i].rz / -8.6)) + 230;
			RETRO_PutPixel(x, y, color);
		}
	}

	RETRO_Blur(RETRO_BLUR_VERTICAL);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_WHITE);

	// Create Sphere (uniform sampling, no clustering at the poles)
	for (int i = 0; i < POINTS; i++) {
		float z = 1 - RANDOMF(2);
		float r = sqrt(1 - z * z);
		float phi = RANDOMF(2 * M_PI);
		Sphere[i].x = r * cos(phi);
		Sphere[i].y = r * sin(phi);
		Sphere[i].z = z;
	}

	// Create torus
	for (int i = 0; i < POINTS; i++) {
		float theta = RANDOMF(2 * M_PI);
		float phi = RANDOMF(2 * M_PI);
		Torus[i].x = (0.8 + 0.4 * cos(theta)) * cos(phi);
		Torus[i].y = (0.8 + 0.4 * cos(theta)) * sin(phi);
		Torus[i].z = 0.4 * sin(theta);
	}
}
