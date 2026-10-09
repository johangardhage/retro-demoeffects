//
// Flat filled landscape
//
// The same mesh as flatshadedlandscape.cpp with the light taken away: one
// constant color per mesh quad, the color map at the quad's center, so a
// slope is the color the map painted it and nothing else. Each quad spans
// RETRO_TerrainDraw.step cells on a side. The height map is still what builds
// the mesh, and the depth buffer still resolves it, but no normal is taken and
// no shade table stands between the map and the screen.
//
// What is left to read the ground by is the silhouette against the sky, the
// ridges cutting in front of one another, and the color map's own aerial
// shading, which was photographed with the sun already in it. A ramp of eight
// palette-matched brightness levels is what the shaded version adds on top,
// and the two side by side are the whole of what a lambert term per face buys.
//
// A triangle reaching from behind the camera into view is cut at the near
// plane rather than dropped, so a camera flown low over the ground still has
// ground under the bottom of the screen. The near plane is brought in close
// for the same reason; it can be, because nothing behind it is projected.
//
// Left/Right turn and Up/Down move along the viewing direction. W/S are
// alternate forward/back controls and A/D strafe. Tab toggles a flycam, in
// which R and F raise and lower the camera. PageUp and PageDown move the
// horizon, which tilts the view up and down.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retroterrain.h"
#include "lib/retropalette.h"

// Near enough that a camera flown down onto the ground still sees it
#define LANDSCAPE_NEARPLANE 0.25f

static void DrawCell(const RETRO_TerrainMesh &mesh, const RETRO_TerrainCell &cell, const void *)
{
	unsigned char color = RETRO_TerrainCellColor(mesh, cell);
	for (const int *t : RETRO_TerrainCellTriangles) {
		RETRO_ProjectedTriangle projected;
		RETRO_CameraClipProjectTriangle(mesh.camera.lens, cell.corner[t[0]], cell.corner[t[1]], cell.corner[t[2]], &projected);
		if (projected.count >= 3) RETRO_DrawFlatPolygon(projected.point, projected.count, color);
	}
}

void DEMO_Render(RETRO_Time time)
{
	RETRO_UpdateTerrainRider(time.delta);
	RETRO_ClearDepthBuffer();
	RETRO_WalkTerrainMesh(RETRO_BuildTerrainMesh(), DrawCell);
}

void DEMO_Initialize(void)
{
	// The map's own palette is the palette: with no shading there is nothing to
	// match darkened colors back to, and every entry stays the color it was
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// Sky, in an entry the color map never uses
	RETRO_SetColor(0, RETRO_NIGHTSKY);

	RETRO_Rider.lens.nearplane = LANDSCAPE_NEARPLANE;
	RETRO_Rider.truepitch = true; // PageUp/PageDown tip the view
	RETRO_PlaceTerrainRider(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
