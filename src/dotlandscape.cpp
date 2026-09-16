//
// Dot landscape
//
// The 256x256 voxel_height_256x256.pcx height field, paired with its own
// voxel_color_256x256.pcx color map, tiled without limit and rendered as a
// point cloud in perspective. The same look and PlotDot as dotscroller3.cpp,
// which pairs the equivalent 128x128 pair with a scrolling text strip and a
// turning camera; here the camera cruises straight ahead instead.
//
// There are no controls.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retropoly.h"

#define CAMERA_HEIGHT 20.0f
#define PITCH -0.5f
#define NEAR_PLANE 1.0f
#define VIEW_DISTANCE 100.0f
#define WORLD_HEIGHT_SCALE (1.0f / 16.0f)
#define FORWARD_SPEED 30.0f

static RETRO_Image *HeightMap, *ColorMap;

static unsigned char TerrainSample(int x, int z)
{
	return HeightMap->data[WRAP(z, HeightMap->height) * HeightMap->width + WRAP(x, HeightMap->width)];
}

static unsigned char ColorSample(int x, int z)
{
	return ColorMap->data[WRAP(z, ColorMap->height) * ColorMap->width + WRAP(x, ColorMap->width)];
}

// Terrain and letters use the same perspective and pixel depth buffer.
static void PlotDot(float side, float forward, float height, unsigned char color)
{
	float up = (height - CAMERA_HEIGHT) * cosf(PITCH) - forward * sinf(PITCH);
	float depth = (height - CAMERA_HEIGHT) * sinf(PITCH) + forward * cosf(PITCH);
	if (depth < NEAR_PLANE || depth > VIEW_DISTANCE) return;
	float sx = RETRO_WIDTH * 0.5f + side * (RETRO_WIDTH * 0.5f) / depth;
	float sy = RETRO_HEIGHT * 0.5f - up * (RETRO_HEIGHT * 0.5f) / depth;
	if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) return;
	int x = (int)sx, y = (int)sy;
	if (RETRO_DepthTest(y * RETRO_WIDTH + x, 1.0f / depth)) {
		RETRO_PutPixel(x, y, color);
	}
}

// Scan the view radius around the camera and plot every terrain cell in it.
static void DrawTerrainDots(float camerax, float cameraz)
{
	int minx = (int)floorf(camerax - VIEW_DISTANCE);
	int maxx = (int)ceilf(camerax + VIEW_DISTANCE);
	int minz = (int)floorf(cameraz - VIEW_DISTANCE);
	int maxz = (int)ceilf(cameraz + VIEW_DISTANCE);
	for (int z = minz; z <= maxz; z++) {
		for (int x = minx; x <= maxx; x++) {
			float dx = x - camerax, dz = z - cameraz;
			// No heading to turn by: this camera cruises straight, unlike dotscroller3's.
			float side = dx;
			float forward = dz;
			PlotDot(side, forward, TerrainSample(x, z) * WORLD_HEIGHT_SCALE, ColorSample(x, z));
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	static float camerax = 128, cameraz = 236;
	cameraz = fmodf(cameraz + FORWARD_SPEED * deltatime + HeightMap->height, HeightMap->height);

	RETRO_ClearDepthBuffer();
	DrawTerrainDots(camerax, cameraz);
}

void DEMO_Initialize(void)
{
	HeightMap = RETRO_LoadImage("assets/voxel_height_256x256.pcx");
	ColorMap = RETRO_LoadImage("assets/voxel_color_256x256.pcx", true);
	RETRO_SetColor(0, RETRO_NIGHTSKY);
}
