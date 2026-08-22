//
// Gouraud-shaded landscape
//
// The 1024x1024 voxel height map drawn as a wrapping triangle mesh. A central
// height difference gives every shared vertex a normal; its light is then
// interpolated across the triangle. Three contiguous palette ramps keep that
// interpolation within water, vegetation or earth.
//
// Three and not four: a snow ramp was classified for as well, and this map has
// no snow to put in it. Two of the 256 source entries came out bright enough
// and grey enough to be called snow, one of them appears on the map at all, and
// it covers a thousandth of it. A quarter of the palette went to that, and the
// three materials the map is actually made of divided up what was left.
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
#include "lib/retrogfx.h"
#include "lib/retropoly.h"
#include "lib/retroterrain.h"

#define LANDSCAPE_SHADES 8
#define LANDSCAPE_MATERIALS 3

// One palette index past the last of each material's ramp. Entry 0 is the sky
// and belongs to no material, so the first ramp begins one past it.
static const int MaterialRampEnd[LANDSCAPE_MATERIALS] = { 85, 170, RETRO_COLORS };

unsigned char MaterialTable[RETRO_COLORS];

struct ShadedVertex {
	float sx, sy, q;
	unsigned char shade;
};

static ShadedVertex ProjectVertex(float x, float z, int step, const RETRO_TerrainBasis &basis)
{
	RETRO_TerrainPoint point = RETRO_ProjectTerrainVertex(x, z, basis);
	ShadedVertex vertex;
	vertex.sx = point.sx;
	vertex.sy = point.sy;
	vertex.q = point.q;

	// Central differences make the same normal whenever this map vertex is
	// visited from an adjacent cell, so shared triangle edges stay continuous.
	float nx = RETRO_TerrainHeight(x - step, z) - RETRO_TerrainHeight(x + step, z);
	float ny = 2.0f * step;
	float nz = RETRO_TerrainHeight(x, z - step) - RETRO_TerrainHeight(x, z + step);
	vertex.shade = RETRO_TerrainShade(nx, ny, nz, LANDSCAPE_SHADES);
	return vertex;
}

static void DrawTriangle(const ShadedVertex &a, const ShadedVertex &b, const ShadedVertex &c, unsigned char basecolor)
{
	// RETRO_DrawGouraudPolygon interpolates palette indices, so each material
	// owns one consecutive ramp and a triangle stays inside its own.
	int material = MaterialTable[basecolor];
	int first = material == 0 ? 1 : MaterialRampEnd[material - 1];
	int last = MaterialRampEnd[material] - 1;
	float shadescale = (last - first) / (float)(LANDSCAPE_SHADES - 1);
	PolygonPoint polygon[3] = {
		{ a.sx, a.sy, first + a.shade * shadescale, 0, 0, a.q, 0, 0, 0 },
		{ b.sx, b.sy, first + b.shade * shadescale, 0, 0, b.q, 0, 0, 0 },
		{ c.sx, c.sy, first + c.shade * shadescale, 0, 0, c.q, 0, 0, 0 }
	};
	RETRO_DrawGouraudPolygon(polygon, 3);
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

			ShadedVertex p00 = ProjectVertex(x, z, step, mesh.basis);
			ShadedVertex p10 = ProjectVertex(x + step, z, step, mesh.basis);
			ShadedVertex p01 = ProjectVertex(x, z + step, step, mesh.basis);
			ShadedVertex p11 = ProjectVertex(x + step, z + step, step, mesh.basis);
			if (!RETRO_TerrainCellProjects(mesh, p00.q, p10.q, p01.q, p11.q)) continue;

			unsigned char color = RETRO_TerrainColor(x + step / 2.0f, z + step / 2.0f);
			DrawTriangle(p00, p11, p10, color);
			DrawTriangle(p00, p01, p11, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// Classify the source palette into terrain materials before replacing the
	// active palette with the ramps used by Gouraud interpolation. Blue against
	// its own red and green is water and green against both is vegetation;
	// everything else, the map's greys and its bare rock included, is earth.
	RETRO_Palette *palette = RETRO_ImagePalette(0);
	for (int color = 0; color < RETRO_COLORS; color++) {
		RETRO_Palette p = palette[color];
		if (p.b > p.r * 1.15f && p.b > p.g * 1.05f) MaterialTable[color] = 0;
		else if (p.g > p.r * 1.05f && p.g > p.b) MaterialTable[color] = 1;
		else MaterialTable[color] = 2;
	}
	RETRO_CreateGradientPalette(1, MaterialRampEnd[0], RETRO_Palette{ 3, 10, 22 }, RETRO_Palette{ 70, 180, 235 });
	RETRO_CreateGradientPalette(MaterialRampEnd[0], MaterialRampEnd[1], RETRO_Palette{ 7, 20, 8 }, RETRO_Palette{ 155, 205, 90 });
	RETRO_CreateGradientPalette(MaterialRampEnd[1], MaterialRampEnd[2], RETRO_Palette{ 22, 14, 10 }, RETRO_Palette{ 205, 170, 120 });
	RETRO_SetColor(0, 20, 24, 42);
	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
