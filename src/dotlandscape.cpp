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
#include "lib/retroterrain.h"

#define CAMERA_X 128.0f
#define CAMERA_HEIGHT 20.0f
#define PITCH -0.5f
#define NEAR_PLANE 1.0f
#define VIEW_DISTANCE 100.0f
#define WORLD_HEIGHT_SCALE (1.0f / 16.0f)
#define FORWARD_SPEED 30.0f

// The pitched camera's perspective, into a pixel depth buffer.
static void PlotDot(float side, float forward, float height, unsigned char color)
{
	float up = (height - CAMERA_HEIGHT) * cosf(PITCH) - forward * sinf(PITCH);
	float depth = (height - CAMERA_HEIGHT) * sinf(PITCH) + forward * cosf(PITCH);
	if (depth < NEAR_PLANE || depth > VIEW_DISTANCE) return;
	float sx = RETRO_WIDTH * 0.5f + side * (RETRO_WIDTH * 0.5f) / depth;
	float sy = RETRO_HEIGHT * 0.5f - up * (RETRO_HEIGHT * 0.5f) / depth;
	if (!RETRO_OnScreen(sx, sy)) return;
	int x = (int)sx, y = (int)sy;
	if (RETRO_DepthTest(y * RETRO_WIDTH + x, 1.0f / depth)) {
		RETRO_PutPixel(x, y, color);
	}
}

// Scan the view radius around the camera and plot every terrain cell in it.
static void DrawTerrainDots(float cameraz)
{
	int minx = (int)floorf(CAMERA_X - VIEW_DISTANCE);
	int maxx = (int)ceilf(CAMERA_X + VIEW_DISTANCE);
	int minz = (int)floorf(cameraz - VIEW_DISTANCE);
	int maxz = (int)ceilf(cameraz + VIEW_DISTANCE);
	for (int z = minz; z <= maxz; z++) {
		for (int x = minx; x <= maxx; x++) {
			// The camera cruises straight ahead, so a cell's offset from it is
			// already its side and forward distance.
			PlotDot(x - CAMERA_X, z - cameraz, RETRO_TerrainHeight(x, z), RETRO_TerrainColor(x, z));
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	static float cameraz = 236;
	cameraz = fmodf(cameraz + FORWARD_SPEED * deltatime + RETRO_Terrain.height, RETRO_Terrain.height);

	RETRO_ClearDepthBuffer();
	DrawTerrainDots(cameraz);
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_256x256.pcx", "assets/voxel_height_256x256.pcx", WORLD_HEIGHT_SCALE);
	RETRO_SetColor(0, RETRO_NIGHTSKY);
}
