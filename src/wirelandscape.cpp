//
// Wireframe landscape
//
// The same mesh as flatlandscape.cpp drawn as a grid: only the edges of the
// mesh quads are drawn, as lines, and the quads themselves are filled with the
// color of the sky. A filled quad shows nothing of itself, but it covers what
// is behind it, so the far side of a hill and the ground beyond a ridge are
// hidden and the grid reads as a solid surface rather than as a tangle of
// lines. Each quad spans RETRO_TerrainView.step cells on a side.
//
// A line has no depth to test against a depth buffer with, so the quads are
// drawn in order instead, the farthest first, and each is filled and then
// outlined, so that a nearer quad paints over the lines of those behind it.
// On a grid the order needs no sorting. A quad can only be hidden by quads
// that lie between it and the camera, and those are in rows and columns no
// further from the camera's own than its row and column are. So the rows are
// taken from the far ends of the mesh in toward the row the camera is over,
// that row last, and the quads of a row the same way.
//
// A line is as bright as it is near: its color fades from LINE_NEAR through
// LINE_MIDDLE to the sky's own at the draw distance, where the grid is lost
// in the dark rather than cut off, and where the lines would otherwise crowd
// into a solid band under the horizon.
//
// A triangle or a line reaching from behind the camera into view is cut at
// the near plane rather than dropped, so a camera flown low over the ground
// still has ground under the bottom of the screen.
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
#include "lib/retropalette.h"

// Near enough that a camera flown down onto the ground still sees it
#define LANDSCAPE_NEARPLANE 0.25f
#define LANDSCAPE_STEP 16 // map cells to a side of a quad
#define LANDSCAPE_DISTANCE 420 // map cells the grid is drawn out to

#define SKY 0 // palette entry of the sky, and of the quads
#define LINE_RAMP0 1 // palette entry the ramp of the lines starts at
#define LINE_SHADES 48 // entries in it, from the sky's color to LINE_NEAR
#define LINE_MIDDLE RETRO_Palette{ 0, 150, 255 }
#define LINE_NEAR RETRO_Palette{ 200, 255, 255 }

static RETRO_CameraVertex TerrainVertex(float x, float z, const RETRO_TerrainBasis &basis)
{
	RETRO_CameraVertex vertex = {};
	vertex.eye = RETRO_TerrainPointEye(x, z, RETRO_TerrainHeight(x, z), basis);
	return vertex;
}

static void DrawTriangle(const RETRO_CameraVertex &a, const RETRO_CameraVertex &b, const RETRO_CameraVertex &c, unsigned char color)
{
	RETRO_CameraVertex triangle[3] = { a, b, c };
	PolygonPoint polygon[4];
	int points = RETRO_ClipProjectViewPolygon(RETRO_TerrainLens, triangle, 3, polygon);
	if (points < 3) return;

	RETRO_DrawFlatPolygon(polygon, points, color);
}

//
// The point of the line from behind to front that is on the near plane
//
static vec3 CutAtNearPlane(vec3 behind, vec3 front)
{
	float t = (RETRO_TerrainLens.nearplane - behind.z) / (front.z - behind.z);
	return {
		mix(behind.x, front.x, t),
		mix(behind.y, front.y, t),
		RETRO_TerrainLens.nearplane,
	};
}

static void DrawLine(vec3 a, vec3 b)
{
	float nearplane = RETRO_TerrainLens.nearplane;
	if (a.z < nearplane && b.z < nearplane) return;

	// The color is taken at the middle of the whole line
	float distance = length(vec2{ a.x + b.x, a.z + b.z }) / 2;
	float fade = 1 - distance / RETRO_TerrainView.distance;
	unsigned char color = LINE_RAMP0 + CLAMP((int)(fade * LINE_SHADES), 0, LINE_SHADES);

	if (a.z < nearplane) {
		a = CutAtNearPlane(a, b);
	} else if (b.z < nearplane) {
		b = CutAtNearPlane(b, a);
	}

	PolygonPoint from = RETRO_ProjectViewPoint(RETRO_TerrainLens, a);
	PolygonPoint to = RETRO_ProjectViewPoint(RETRO_TerrainLens, b);
	RETRO_DrawLine((int)floorf(from.pos.x), (int)floorf(from.pos.y), (int)floorf(to.pos.x), (int)floorf(to.pos.y), color);
}

//
// The i'th of the rows or columns of the mesh from min up to max, taken from
// its two ends in toward the one the camera is over
//
static int FarFirst(int i, int min, int max, int camera, int step)
{
	int before = (camera - min) / step;
	return i < before ? min + i * step : max - step - (i - before) * step;
}

void DEMO_Render(RETRO_Time time)
{
	RETRO_UpdateTerrainCamera(time.delta);
	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	int step = mesh.step;

	// The quad the camera is over, and the rows and columns of the mesh
	int camerax = (int)floorf(RETRO_TerrainCamera.x / step) * step;
	int cameraz = (int)floorf(RETRO_TerrainCamera.z / step) * step;
	int columns = (mesh.maxx - mesh.minx) / step;
	int rows = (mesh.maxz - mesh.minz) / step;

	RETRO_ClearDepthBuffer();
	for (int j = 0; j < rows; j++) {
		int z = FarFirst(j, mesh.minz, mesh.maxz, cameraz, step);

		for (int i = 0; i < columns; i++) {
			int x = FarFirst(i, mesh.minx, mesh.maxx, camerax, step);
			if (!RETRO_TerrainCellVisible(mesh, x, z)) continue;

			RETRO_CameraVertex p00 = TerrainVertex(x, z, mesh.basis);
			RETRO_CameraVertex p10 = TerrainVertex(x + step, z, mesh.basis);
			RETRO_CameraVertex p01 = TerrainVertex(x, z + step, mesh.basis);
			RETRO_CameraVertex p11 = TerrainVertex(x + step, z + step, mesh.basis);

			DrawTriangle(p00, p11, p10, SKY);
			DrawTriangle(p00, p01, p11, SKY);

			DrawLine(p00.eye, p10.eye);
			DrawLine(p10.eye, p11.eye);
			DrawLine(p11.eye, p01.eye);
			DrawLine(p01.eye, p00.eye);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// Init palette: the sky, and the ramp of the lines from the sky's color
	// through LINE_MIDDLE to LINE_NEAR
	RETRO_SetColor(SKY, RETRO_NIGHTSKY);
	RETRO_CreateGradientPalette(LINE_RAMP0, LINE_RAMP0 + LINE_SHADES / 2, RETRO_NIGHTSKY, LINE_MIDDLE);
	RETRO_CreateGradientPalette(LINE_RAMP0 + LINE_SHADES / 2, LINE_RAMP0 + LINE_SHADES, LINE_MIDDLE, LINE_NEAR);

	RETRO_TerrainView.step = LANDSCAPE_STEP;
	RETRO_TerrainView.distance = LANDSCAPE_DISTANCE;
	RETRO_TerrainLens.nearplane = LANDSCAPE_NEARPLANE;
	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
