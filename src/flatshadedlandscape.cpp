//
// Flat-shaded landscape
//
// The 1024x1024 voxel height and color maps drawn as a wrapping triangle mesh.
// Each mesh quad spans RETRO_TerrainView.step cells on a side and becomes two
// flat-shaded polygons. Their color is the color map at the quad's centre and
// their light is the triangle normal, quantized through eight palette-matched
// brightness levels. A depth buffer resolves the mesh without requiring a
// painter's sort.
//
// flatlandscape.cpp is the same mesh with the light taken away.
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
// How much light a face turned away from the sun still catches, as a fraction
// of full brightness. See the shade table below for why it is not zero
#define LANDSCAPE_AMBIENT 0.40f

unsigned char ShadeTable[RETRO_COLORS][LANDSCAPE_SHADES];

struct WorldVertex {
	float x, y, z;
	float sx, sy, q;
};

// The world position is kept alongside the screen one: the face normal below is
// taken in world space, which the projected point no longer carries.
static WorldVertex ProjectVertex(float x, float z, const RETRO_TerrainBasis &basis)
{
	WorldVertex vertex;
	vertex.x = x;
	vertex.y = RETRO_TerrainHeight(x, z);
	vertex.z = z;

	RETRO_TerrainPoint point = RETRO_ProjectTerrainPoint(x, z, vertex.y, basis);
	vertex.sx = point.sx;
	vertex.sy = point.sy;
	vertex.q = point.q;
	return vertex;
}

static void DrawTriangle(const WorldVertex &a, const WorldVertex &b, const WorldVertex &c, unsigned char basecolor)
{
	// The unnormalized cross product supplies both the face normal and its
	// area. A height field over a regular grid fixes the vertical component at
	// the mesh spacing squared for either winding, since it is built from the x
	// and z spacing alone, so the normal already points upward and the length
	// is never zero.
	float abx = b.x - a.x;
	float aby = b.y - a.y;
	float abz = b.z - a.z;
	float acx = c.x - a.x;
	float acy = c.y - a.y;
	float acz = c.z - a.z;
	float nx = aby * acz - abz * acy;
	float ny = abz * acx - abx * acz;
	float nz = abx * acy - aby * acx;
	int shade = RETRO_TerrainShade(nx, ny, nz, LANDSCAPE_SHADES);

	PolygonPoint polygon[3] = {
		{ a.sx, a.sy, 0, 0, 0, a.q, 0, 0, 0 },
		{ b.sx, b.sy, 0, 0, 0, b.q, 0, 0, 0 },
		{ c.sx, c.sy, 0, 0, 0, c.q, 0, 0, 0 }
	};
	RETRO_DrawFlatPolygon(polygon, 3, ShadeTable[basecolor][shade]);
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

			WorldVertex p00 = ProjectVertex(x, z, mesh.basis);
			WorldVertex p10 = ProjectVertex(x + step, z, mesh.basis);
			WorldVertex p01 = ProjectVertex(x, z + step, mesh.basis);
			WorldVertex p11 = ProjectVertex(x + step, z + step, mesh.basis);
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

	// Match each darkened source color back to the nearest entry in the map's
	// own palette. Polygons can therefore be lit without replacing its terrain
	// colors with a generic material ramp, which is the trade gouraudlandscape.cpp
	// makes instead.
	//
	// The ambient floor is what makes that affordable. A photographed palette
	// carries a picture's colors and not ramps, so an entry darkened towards
	// black finds nothing near it but the few dark colors the picture happened
	// to contain, and the face it was lighting drops to one of those: a hole in
	// the hillside rather than a shadow on it. Starting the darkening partway up
	// keeps every shade among colors the map has plenty of.
	//
	// It buys resolution as well as safety. The map uses 184 of the 256 entries,
	// and darkening them the whole way to black leaves 168 of those with fewer
	// than eight distinct matches, so all but a fiftieth of the map is drawn in
	// ramps that repeat themselves. Darkening only to the floor cuts that to 93
	// entries, and to under half the map.
	RETRO_CreatePaletteShadeTable(RETRO_ImagePalette(0), RETRO_COLORS, LANDSCAPE_SHADES, &ShadeTable[0][0], LANDSCAPE_AMBIENT);

	// Sky, in an entry the color map never uses. The table above matched against
	// the image's own palette, which keeps its copy of entry 0, so this changes
	// only what the entry displays as.
	RETRO_SetColor(0, 20, 24, 42);

	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
