//
// plane.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retro3d2.h"

#define PLANE_DISTANCE 320

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame = 0;
	frame += deltatime * 20;

	Point3Df bp = { (float)frame, -16, (float)frame };
	Point3Df up = { 256.0f, 0, 0 };
	Point3Df vp = { 0, 0, 256.0f };

	unsigned char *image = RETRO_GetTextureImage();

	// Draw a perspective correct textured plane
	float cx = up.y * vp.z - vp.y * up.z;
	float cy = vp.x * up.z - up.x * vp.z;
	float cz = (up.x * vp.y - vp.x * up.y) * PLANE_DISTANCE;
	float ax = vp.y * bp.z - bp.y * vp.z;
	float ay = bp.x * vp.z - vp.x * bp.z;
	float az = (vp.x * bp.y - bp.x * vp.y) * PLANE_DISTANCE;
	float bx = bp.y * up.z - up.y * bp.z;
	float by = up.x * bp.z - bp.x * up.z;
	float bz = (bp.x * up.y - up.x * bp.y) * PLANE_DISTANCE;

	// Only render the lower part of the plane
	for (int y = 140; y < RETRO_HEIGHT; y++) {
		// Compute the (U,V) coordinates and the interpolation
		float a = az + ay * (y - (RETRO_HEIGHT / 2)) + ax * -(RETRO_WIDTH / 2);
		float b = bz + by * (y - (RETRO_HEIGHT / 2)) + bx * -(RETRO_WIDTH / 2);
		float c = cz + cy * (y - (RETRO_HEIGHT / 2)) + cx * -(RETRO_WIDTH / 2);

		float ic = fabs(c) > 65536 ? 1 / c : 1 / 65536;

		int u = (int) (a * 16777216 * ic);
		int v = (int) (b * 16777216 * ic);
		int du = (int) (16777216 * ax * ic);
		int dv = (int) (16777216 * bx * ic);

		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char color = image[((v >> 8) & 0xff00) + ((u >> 16) & 0xff)];
			RETRO_PutPixel(x, y, color);

			u += du;
			v += dv;
		}
	}
}

void DEMO_Initialize()
{
	RETRO_LoadTexture("assets/plane_256x256.pcx");
	RETRO_SetPaletteFromTexture();
}
