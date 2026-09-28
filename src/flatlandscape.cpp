//
// Flat filled landscape
//
// The same mesh as flatshadedlandscape.cpp with the light taken away: one
// constant color per mesh quad, the color map at the quad's centre, so a
// slope is the color the map painted it and nothing else. Each quad spans
// RETRO_TerrainView.step cells on a side. The height map is still what builds
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

static RETRO_TerrainVertex TerrainVertex(float x, float z, const RETRO_TerrainBasis &basis)
{
	RETRO_TerrainVertex vertex = {};
	vertex.eye = RETRO_TerrainPointEye(x, z, RETRO_TerrainHeight(x, z), basis);
	return vertex;
}

static void DrawTriangle(const RETRO_TerrainVertex &a, const RETRO_TerrainVertex &b, const RETRO_TerrainVertex &c, unsigned char color)
{
	RETRO_TerrainVertex triangle[3] = { a, b, c };
	PolygonPoint polygon[4];
	int points = RETRO_ClipProjectTerrainPolygon(triangle, 3, polygon);
	if (points < 3) return;

	RETRO_DrawFlatPolygon(polygon, points, color);
}

void DEMO_Render(double time, double deltatime)
{
	RETRO_UpdateTerrainCamera(deltatime);
	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	int step = mesh.step;

	RETRO_ClearDepthBuffer();
	for (int z = mesh.minz; z < mesh.maxz; z += step) {
		for (int x = mesh.minx; x < mesh.maxx; x += step) {
			if (!RETRO_TerrainCellVisible(mesh, x, z)) continue;

			RETRO_TerrainVertex p00 = TerrainVertex(x, z, mesh.basis);
			RETRO_TerrainVertex p10 = TerrainVertex(x + step, z, mesh.basis);
			RETRO_TerrainVertex p01 = TerrainVertex(x, z + step, mesh.basis);
			RETRO_TerrainVertex p11 = TerrainVertex(x + step, z + step, mesh.basis);

			unsigned char color = RETRO_TerrainColor(x + step / 2.0f, z + step / 2.0f);
			DrawTriangle(p00, p11, p10, color);
			DrawTriangle(p00, p01, p11, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// The map's own palette is the palette: with no shading there is nothing to
	// match darkened colors back to, and every entry stays the color it was
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// Sky, in an entry the color map never uses
	RETRO_SetColor(0, RETRO_NIGHTSKY);

	RETRO_TerrainView.nearplane = LANDSCAPE_NEARPLANE;
	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
