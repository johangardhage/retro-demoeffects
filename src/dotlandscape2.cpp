//
// Rotating dot landscape
//
// The 128x128 voxel_height_128x128.pcx landscape, paired with its own
// voxel_color_128x128.pcx colormap, rendered as a field of dots. The island
// look is the terrain library's: the camera is pitched down so the island
// fills the frame, the patch turns about its center, and the camera dollies
// along the viewing axis between stops that keep the finite patch in view.
// The same look as dotscroller4.cpp, which shares these assets.
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
	RETRO_UpdateTerrainIsland(time.delta);
	RETRO_ClearDepthBuffer();

	// The finite 128x128 terrain, every cell a dot, through the island look
	RETRO_Camera camera = RETRO_CameraFromIsland();
	for (int z = 0; z < RETRO_Terrain.height; z++) {
		for (int x = 0; x < RETRO_Terrain.width; x++) {
			vec3 eye = RETRO_ViewPoint(&camera, { (float)x, RETRO_TerrainHeight(x, z), (float)z });
			PlotDot(camera.lens, eye, RETRO_TerrainColor(x, z));
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_128x128.pcx", "assets/voxel_height_128x128.pcx", WORLD_HEIGHT_SCALE, false);
	RETRO_SetColor(0, RETRO_NIGHTSKY);
	RETRO_PlaceTerrainIsland();
	RETRO_Island.rotation = ISLAND_START_ROTATION;
}
