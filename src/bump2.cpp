//
// Bump mapping, shaded per pixel
//
// The same height map as bump.cpp, lit by Blinn-Phong at every pixel. The
// frame is the screen: y grows down, z toward the viewer (orthographic).
// The height field h(x, y) is the surface (x, y, h). Its tangents are
// (1, 0, hx) and (0, 1, hy), so
//
//   N = (−hx, −hy, 1) / |…|
//
// built once. Slopes are second-order: interior (h₊ − h₋)/2, border the
// one-sided (−3h₀ + 4h₁ − h₂)/2. Each frame a point light at
// (lx, ly, LIGHT_HEIGHT) gives
//
//   L = normalize((lx − x, ly − y, LIGHT_HEIGHT))
//   H = normalize(L + (0, 0, 1))
//   I = att · (kd max(N·L, 0) + ks max(N·H, 0)^n)
//
// The light rides a 1:2 Lissajous, so its angle lives on 360°.
//
// Specular is gated on N·L > 0 (front faces only). att = H² / |ℓ|², so
// attenuation is 1 at the foot of the perpendicular. kd and ks are
// independent, so I can exceed 1; clamping to the ramp is the white core
// of the highlight. n stays low because the height map has relief on the
// scale of a pixel: a tight lobe falls between samples.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define LIGHT_COLORS 128 // palette entries the shading ramps over
#define LIGHT_ORBIT 128 // radius of the light's path across the screen, in pixels
#define LIGHT_SPEED 100 // degrees of that path per second
#define LIGHT_HEIGHT 80 // how far the light stands off the surface, in pixels

#define BUMP_HEIGHT_SCALE (1.0 / 16.0) // pixels of height per height map unit, so how deep the relief reads

#define MATERIAL_DIFFUSE 0.75
#define MATERIAL_SPECULAR 0.50
#define MATERIAL_SHININESS 8 // specular exponent, low enough to keep the highlight wider than a pixel

struct SurfaceNormal {
	float x, y, z;
};

SurfaceNormal SurfaceNormals[RETRO_HEIGHT * RETRO_WIDTH];

//
// Slope of the height map along x, second-order finite difference
//
// Interior: (h[x+1] - h[x-1]) / 2. On the border the same order, one-sided:
// (-3 h0 + 4 h1 - h2) / 2 forwards, and its mirror backwards.
//
float HeightSlopeX(unsigned char *heightmap, int x, int y)
{
	int offset = y * RETRO_WIDTH + x;

	if (x == 0) {
		return (-3.0 * heightmap[offset] + 4.0 * heightmap[offset + 1] - heightmap[offset + 2]) / 2.0;
	} else if (x == RETRO_WIDTH - 1) {
		return (3.0 * heightmap[offset] - 4.0 * heightmap[offset - 1] + heightmap[offset - 2]) / 2.0;
	}
	return (heightmap[offset + 1] - heightmap[offset - 1]) / 2.0;
}

float HeightSlopeY(unsigned char *heightmap, int x, int y)
{
	int offset = y * RETRO_WIDTH + x;

	if (y == 0) {
		return (-3.0 * heightmap[offset] + 4.0 * heightmap[offset + RETRO_WIDTH] - heightmap[offset + 2 * RETRO_WIDTH]) / 2.0;
	} else if (y == RETRO_HEIGHT - 1) {
		return (3.0 * heightmap[offset] - 4.0 * heightmap[offset - RETRO_WIDTH] + heightmap[offset - 2 * RETRO_WIDTH]) / 2.0;
	}
	return (heightmap[offset + RETRO_WIDTH] - heightmap[offset - RETRO_WIDTH]) / 2.0;
}

void DEMO_Render(double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Calculate light
	static double angle = 0;
	angle = fmod(angle + deltatime * LIGHT_SPEED, RETRO_DEGREES_PER_TURN);

	float lx = RETRO_WIDTH / 2 + LIGHT_ORBIT * cos(angle * DEG2RAD);
	float ly = RETRO_HEIGHT / 2 + LIGHT_ORBIT * sin(2 * angle * DEG2RAD);

	// Draw bump
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;
			SurfaceNormal normal = SurfaceNormals[offset];

			float lightx = lx - x;
			float lighty = ly - y;
			float distancesquared = lightx * lightx + lighty * lighty + LIGHT_HEIGHT * LIGHT_HEIGHT;
			float inversedistance = 1.0 / sqrt(distancesquared);

			float lightdirx = lightx * inversedistance;
			float lightdiry = lighty * inversedistance;
			float lightdirz = LIGHT_HEIGHT * inversedistance;

			float cosangle = normal.x * lightdirx + normal.y * lightdiry + normal.z * lightdirz;
			float lambert = CLAMP01(cosangle);

			float specular = 0;
			if (lambert > 0) {
				float halfwayx = lightdirx;
				float halfwayy = lightdiry;
				float halfwayz = lightdirz + 1;
				float inverselength = 1.0 / sqrt(halfwayx * halfwayx + halfwayy * halfwayy + halfwayz * halfwayz);

				halfwayx *= inverselength;
				halfwayy *= inverselength;
				halfwayz *= inverselength;

				float normalhalfway = normal.x * halfwayx + normal.y * halfwayy + normal.z * halfwayz;

				specular = pow(CLAMP01(normalhalfway), MATERIAL_SHININESS);
			}

			float attenuation = LIGHT_HEIGHT * LIGHT_HEIGHT / distancesquared;
			float intensity = attenuation * (MATERIAL_DIFFUSE * lambert + MATERIAL_SPECULAR * specular);

			buffer[offset] = (LIGHT_COLORS - 1) * CLAMP01(intensity) + 0.5;
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *heightmap = RETRO_LoadImage("assets/bump_320x240.pcx");
	if (heightmap->width != RETRO_WIDTH || heightmap->height != RETRO_HEIGHT) {
		RETRO_RageQuit("The height map must be the size of the screen\n");
	}

	// Init palette
	RETRO_CreateGradientPalette(0, LIGHT_COLORS * 3 / 4, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(LIGHT_COLORS * 3 / 4, LIGHT_COLORS, RETRO_RED, RETRO_WHITE);

	// Init normals
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;

			float dhdx = BUMP_HEIGHT_SCALE * HeightSlopeX(heightmap->data, x, y);
			float dhdy = BUMP_HEIGHT_SCALE * HeightSlopeY(heightmap->data, x, y);
			float inverselength = 1.0 / sqrt(dhdx * dhdx + dhdy * dhdy + 1);

			SurfaceNormals[offset].x = -dhdx * inverselength;
			SurfaceNormals[offset].y = -dhdy * inverselength;
			SurfaceNormals[offset].z = inverselength;
		}
	}
}
