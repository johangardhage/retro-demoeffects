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
#include "lib/retropalette.h"
#include "lib/retropoly.h"
#include "lib/retroterrain.h"

// One dot, already projected into the lens's view by the walk
static void PlotDot(int x, int z, vec3, PolygonPoint point, const void *)
{
	int sx = (int)floorf(point.pos.x);
	int sy = (int)floorf(point.pos.y);
	if (RETRO_DepthTest(sy * RETRO_WIDTH + sx, point.q)) RETRO_PutPixel(sx, sy, RETRO_TerrainColor(x, z));
}

void DEMO_Render(RETRO_Time time)
{
	RETRO_UpdateTerrainRider(time.delta);
	RETRO_ClearDepthBuffer();

	// A smoothly distance-thinned grid inside the view radius
	RETRO_Camera view = RETRO_CameraFromRider();
	RETRO_WalkTerrainDots(&view, RETRO_TerrainDraw.distance, PlotDot);
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");
	RETRO_SetColor(0, RETRO_NIGHTSKY);
	RETRO_Rider.truepitch = true; // PageUp/PageDown tip the view
	RETRO_PlaceTerrainRider(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
