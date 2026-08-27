//
// Endless dot landscape
//
// The 1024x1024 voxel landscape rendered as a perspective point cloud. Unlike
// a voxel renderer, it never joins samples into columns: every terrain texel is
// an independent dot and the empty space between them remains visible.
//
// The map wraps, making the camera's world unbounded. Dot density decreases
// smoothly with distance using a world-anchored pattern, avoiding transition
// rings and preventing dots from swimming when the camera moves.
//
// Left/Right turn and Up/Down move along the new viewing direction. W/S are
// alternate forward/back controls and A/D provide optional strafing. Combined
// movement is normalized, so diagonals have the same speed. By default the
// camera follows the ground; Tab toggles a flycam, in which R and F raise
// and lower the camera. PageUp and PageDown move the horizon, which tilts
// the view up and down.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"
#include "lib/retroterrain.h"

unsigned int DotWorldZBuffer[RETRO_WIDTH * RETRO_HEIGHT];

// Draw a smoothly distance-thinned grid inside the view radius.
static void DrawTerrainDots(float maxdistance)
{
	RETRO_TerrainBasis basis = RETRO_TerrainHeadingBasis(RETRO_Camera.heading);
	float maxdistance2 = maxdistance * maxdistance;
	int minx = (int)floorf(RETRO_Camera.x - maxdistance);
	int maxx = (int)ceilf(RETRO_Camera.x + maxdistance);
	int minz = (int)floorf(RETRO_Camera.z - maxdistance);
	int maxz = (int)ceilf(RETRO_Camera.z + maxdistance);

	for (int z = minz; z <= maxz; z++) {
		float dz = z - RETRO_Camera.z;
		for (int x = minx; x <= maxx; x++) {
			float dx = x - RETRO_Camera.x;
			float radius2 = dx * dx + dz * dz;
			if (radius2 > maxdistance2) continue;

			RETRO_TerrainOffset offset;
			RETRO_TerrainPoint point;
			if (!RETRO_ProjectTerrainDot(x, z, dx, dz, radius2, basis, &offset, &point)) continue;

			int sx = (int)point.sx;
			int sy = (int)point.sy;
			unsigned int idepth = (unsigned int)(offset.depth * 256.0f);
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
	RETRO_UpdateTerrainCamera(deltatime);
	memset(DotWorldZBuffer, 0xFF, sizeof(DotWorldZBuffer));
	DrawTerrainDots(RETRO_TerrainView.distance);
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");
	RETRO_SetColor(0, RETRO_NIGHTSKY);
	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
