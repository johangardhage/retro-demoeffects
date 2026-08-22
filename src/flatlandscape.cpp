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

static void DrawTriangle(const RETRO_TerrainPoint &a, const RETRO_TerrainPoint &b, const RETRO_TerrainPoint &c, unsigned char color)
{
	PolygonPoint polygon[3] = {
		{ a.sx, a.sy, 0, 0, 0, a.q, 0, 0, 0 },
		{ b.sx, b.sy, 0, 0, 0, b.q, 0, 0, 0 },
		{ c.sx, c.sy, 0, 0, 0, c.q, 0, 0, 0 }
	};
	RETRO_DrawFlatPolygon(polygon, 3, color);
}

void DEMO_Render(double deltatime)
{
	RETRO_UpdateTerrainCamera(deltatime);
	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	int step = mesh.step;

	RETRO_ClearDepthBuffer();
	for (int z = mesh.minz; z < mesh.maxz; z += step) {
		for (int x = mesh.minx; x < mesh.maxx; x += step) {
			if (!RETRO_TerrainCellVisible(mesh, x, z)) continue;

			RETRO_TerrainPoint p00 = RETRO_ProjectTerrainVertex(x, z, mesh.basis);
			RETRO_TerrainPoint p10 = RETRO_ProjectTerrainVertex(x + step, z, mesh.basis);
			RETRO_TerrainPoint p01 = RETRO_ProjectTerrainVertex(x, z + step, mesh.basis);
			RETRO_TerrainPoint p11 = RETRO_ProjectTerrainVertex(x + step, z + step, mesh.basis);
			if (!RETRO_TerrainCellProjects(mesh, p00.q, p10.q, p01.q, p11.q)) continue;

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
	RETRO_SetColor(0, 20, 24, 42);

	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
