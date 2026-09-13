//
// Texture mapped landscape
//
// The same mesh as flatlandscape.cpp with the color map read per pixel instead
// of per quad: the map is the texture, sampled perspective-correct across every
// triangle, so a quad shows the ground it actually covers rather than the one
// color its centre happened to land on. Each quad spans RETRO_TerrainView.step
// cells on a side, so that is step by step texels a flat fill was spending a
// single color on.
//
// No light is taken here either. What reads the ground is the same as it is
// there - the silhouette, the ridges cutting in front of one another, and the
// color map's own aerial shading - at the resolution of the map rather than of
// the mesh.
//
// The mesh reaches further than the map is wide, so a cell's world position is
// handed in as its texture coordinate unwrapped and the drawer is asked to fold
// it back onto the map, the same fold the height and the color already take.
// Wrapping the coordinate here instead would be the same picture; leaving the
// drawer to clamp it would smear the map's edge texel over everything the walk
// reaches beyond the map.
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
#include "lib/retrovector.h"

struct WorldVertex {
	vec2 spos;
	float q;
	vec2 uv;
};

static WorldVertex ProjectVertex(float x, float z, const RETRO_TerrainBasis &basis)
{
	RETRO_TerrainPoint point = RETRO_ProjectTerrainVertex(x, z, basis);
	WorldVertex vertex;
	vertex.spos = point.spos;
	vertex.q = point.q;
	vertex.uv = { x, z };
	return vertex;
}

static void DrawTriangle(const WorldVertex &a, const WorldVertex &b, const WorldVertex &c)
{
	if (!RETRO_TerrainTriangleProjects(a.q, b.q, c.q)) return;

	PolygonPoint polygon[3] = {
		{ a.spos, 0, a.uv, a.q },
		{ b.spos, 0, b.uv, b.q },
		{ c.spos, 0, c.uv, c.q }
	};
	RETRO_DrawTexMapPolygon(polygon, 3, RETRO_Terrain.colormap, RETRO_Terrain.width, RETRO_Terrain.height, RETRO_Terrain.wrap);
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

			WorldVertex p00 = ProjectVertex(x, z, mesh.basis);
			WorldVertex p10 = ProjectVertex(x + step, z, mesh.basis);
			WorldVertex p01 = ProjectVertex(x, z + step, mesh.basis);
			WorldVertex p11 = ProjectVertex(x + step, z + step, mesh.basis);

			DrawTriangle(p00, p11, p10);
			DrawTriangle(p00, p01, p11);
		}
	}
}

void DEMO_Initialize(void)
{
	// The map's own palette is the palette: the texture is the color map, and a
	// texel is drawn as the entry it was stored as
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// Sky, in an entry the color map never uses
	RETRO_SetColor(0, RETRO_NIGHTSKY);

	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
