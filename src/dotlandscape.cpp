//
// Rotating dot landscape
//
// A 128x128 landscape sampled from heightmap and colormap image assets and
// rendered as a field of dots. The island look is the terrain library's: the
// camera is pitched down so the island fills the frame, the patch turns about
// its centre, and the camera dollies along the viewing axis between stops that
// keep the finite patch in view. The same look as dotscroller3.cpp.
// Left and Right rotate the terrain. Up/W and Down/S move the camera forward
// and backward.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroterrain.h"

#define MAP_WIDTH 128
#define MAP_HEIGHT 128
#define WORLD_HEIGHT_SCALE 0.34f

// HeightMap stores terrain altitude. ColorMap stores the palette index of the
// corresponding dot, so neither value has to be recalculated while rendering.
unsigned char HeightMap[MAP_WIDTH * MAP_HEIGHT];
unsigned char ColorMap[MAP_WIDTH * MAP_HEIGHT];

// More than one terrain dot may land on the same screen pixel. The depth
// buffer ensures that the nearest one remains visible.
unsigned int DotWorldZBuffer[RETRO_WIDTH * RETRO_HEIGHT];

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
			int sx = (int)point.sx;
			int sy = (int)point.sy;
			if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) continue;

			unsigned int idepth = (unsigned int)(eye.depth * 256.0f);
			int screenindex = sy * RETRO_WIDTH + sx;
			if (idepth < DotWorldZBuffer[screenindex]) {
				DotWorldZBuffer[screenindex] = idepth;
				RETRO_PutPixel(sx, sy, RETRO_TerrainColor(x, z));
			}
		}
	}
}

void DEMO_Render(double deltatime)
{
	RETRO_UpdateTerrainIsland(deltatime);
	memset(DotWorldZBuffer, 0xFF, sizeof(DotWorldZBuffer));
	DrawTerrainDots(RETRO_BuildTerrainIslandFrame());
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");
	RETRO_SetColor(0, RETRO_NIGHTSKY);
	RETRO_DownsampleTerrain(HeightMap, ColorMap, MAP_WIDTH, MAP_HEIGHT);
	RETRO_SetTerrain(MAP_WIDTH, MAP_HEIGHT, WORLD_HEIGHT_SCALE, HeightMap, ColorMap, false);
	RETRO_LookDownAtTerrain();
}
