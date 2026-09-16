//
// Rotating dot landscape
//
// The 128x128 voxel_height_128x128.pcx landscape, paired with its own
// voxel_color_128x128.pcx colormap, rendered as a field of dots. The island
// look is the terrain library's: the camera is pitched down so the island
// fills the frame, the patch turns about its centre, and the camera dollies
// along the viewing axis between stops that keep the finite patch in view.
// The same look as dotscroller3.cpp and dotscroller4.cpp, which share these
// assets.
// Left and Right rotate the terrain. Up/W and Down/S move the camera forward
// and backward.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retropoly.h"
#include "lib/retroterrain.h"

#define WORLD_HEIGHT_SCALE (1.0f / 8.0f) // Same relief as dotscroller4
#define ISLAND_START_ROTATION -0.5f // A modest turn off dead-on at startup, as dotscroller4

// Draw the finite 128x128 terrain through the island look.
static void DrawTerrainDots(const RETRO_TerrainIslandFrame &frame)
{
	int width = RETRO_Terrain.width;
	int height = RETRO_Terrain.height;

	for (int z = 0; z < height; z++) {
		for (int x = 0; x < width; x++) {
			RETRO_TerrainEye eye = RETRO_TerrainIslandEye(x, RETRO_TerrainHeight(x, z), z, frame);
			if (eye.depth <= RETRO_TerrainView.nearplane || fabsf(eye.side) > eye.depth * RETRO_TerrainViewCullSlope()) continue;

			RETRO_TerrainPoint point = RETRO_ProjectTerrainView(eye);
			int sx = (int)point.spos.x;
			int sy = (int)point.spos.y;
			if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) continue;

			if (RETRO_DepthTest(sy * RETRO_WIDTH + sx, 1.0f / eye.depth)) {
				RETRO_PutPixel(sx, sy, RETRO_TerrainColor(x, z));
			}
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	RETRO_UpdateTerrainIsland(deltatime);
	RETRO_ClearDepthBuffer();
	DrawTerrainDots(RETRO_BuildTerrainIslandFrame());
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_128x128.pcx", "assets/voxel_height_128x128.pcx", WORLD_HEIGHT_SCALE, false);
	RETRO_SetColor(0, RETRO_NIGHTSKY);
	RETRO_LookDownAtTerrain();
	RETRO_Island.rotation = ISLAND_START_ROTATION;
}
