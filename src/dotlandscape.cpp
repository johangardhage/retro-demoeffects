//
// Dot landscape
//
// The 256x256 voxel_height_256x256.pcx height field, paired with its own
// voxel_color_256x256.pcx color map, tiled without limit and rendered as a
// point cloud in perspective. The same camera as dotscroller3.cpp,
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

// Looking along +z with x to the right and y up, tipped down by -PITCH: the
// frame the dots were always seen in. Half the screen's width and height
// across a unit of depth. Only its z changes as it cruises.
static RETRO_Camera View;

static void PlotDot(const RETRO_CameraLens &lens, vec3 eye, unsigned char color)
{
	if (eye.z < lens.nearplane) return;
	PolygonPoint point = RETRO_ProjectViewPoint(lens, eye);
	int x = (int)floorf(point.pos.x);
	int y = (int)floorf(point.pos.y);
	if (!RETRO_LensViewContains(lens, x, y)) return;
	if (RETRO_DepthTest(y * RETRO_WIDTH + x, point.q)) RETRO_PutPixel(x, y, color);
}

void DEMO_Render(RETRO_Time time)
{
	View.pos.z = fmodf(View.pos.z + FORWARD_SPEED * time.delta + RETRO_Terrain.height, RETRO_Terrain.height);
	RETRO_ClearDepthBuffer();

	// Scan the view radius around the camera and plot every terrain cell in
	// it, through the pitched camera into a pixel depth buffer
	int minx = (int)floorf(View.pos.x - VIEW_DISTANCE);
	int maxx = (int)ceilf(View.pos.x + VIEW_DISTANCE);
	int minz = (int)floorf(View.pos.z - VIEW_DISTANCE);
	int maxz = (int)ceilf(View.pos.z + VIEW_DISTANCE);
	for (int z = minz; z <= maxz; z++) {
		for (int x = minx; x <= maxx; x++) {
			vec3 eye = RETRO_ViewPoint(&View, { (float)x, RETRO_TerrainHeight(x, z), (float)z });
			if (eye.z <= VIEW_DISTANCE) PlotDot(View.lens, eye, RETRO_TerrainColor(x, z));
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_256x256.pcx", "assets/voxel_height_256x256.pcx", WORLD_HEIGHT_SCALE);
	RETRO_SetColor(0, RETRO_NIGHTSKY);

	View.pos = { CAMERA_X, CAMERA_HEIGHT, 236 };
	RETRO_AimCamera(&View, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, 0, -PITCH);
	View.lens.focalx = RETRO_WIDTH * 0.5f;
	View.lens.focaly = RETRO_HEIGHT * 0.5f;
	View.lens.nearplane = NEAR_PLANE;
}
