//
// Wireframe landscape
//
// The same mesh as flatlandscape.cpp drawn as a grid: only the edges of the
// mesh quads are drawn, as lines, and the quads themselves are filled with the
// color of the sky. A filled quad shows nothing of itself, but it covers what
// is behind it, so the far side of a hill and the ground beyond a ridge are
// hidden and the grid reads as a solid surface rather than as a tangle of
// lines. Each quad spans RETRO_TerrainDraw.step cells on a side.
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

static void DrawLine(const RETRO_CameraLens &lens, vec3 a, vec3 b)
{
	// The color is taken at the middle of the whole line, before it is cut
	float distance = length(vec2{ a.x + b.x, a.z + b.z }) / 2;
	float fade = 1 - distance / RETRO_TerrainDraw.distance;
	unsigned char color = LINE_RAMP0 + CLAMP((int)(fade * LINE_SHADES), 0, LINE_SHADES);

	if (!RETRO_CutViewLine(&a, &b, lens.nearplane)) return;

	PolygonPoint from = RETRO_ProjectViewPoint(lens, a);
	PolygonPoint to = RETRO_ProjectViewPoint(lens, b);
	RETRO_DrawLine((int)floorf(from.pos.x), (int)floorf(from.pos.y), (int)floorf(to.pos.x), (int)floorf(to.pos.y), color);
}

static void DrawCell(const RETRO_TerrainMesh &mesh, const RETRO_TerrainCell &cell, const void *)
{
	const RETRO_CameraVertex &p00 = cell.corner[0], &p10 = cell.corner[1], &p01 = cell.corner[2], &p11 = cell.corner[3];

	for (const int *t : RETRO_TerrainCellTriangles) {
		RETRO_ProjectedTriangle projected;
		RETRO_CameraClipProjectTriangle(mesh.camera.lens, cell.corner[t[0]], cell.corner[t[1]], cell.corner[t[2]], &projected);
		if (projected.count >= 3) RETRO_DrawFlatPolygon(projected.point, projected.count, SKY);
	}

	DrawLine(mesh.camera.lens, p00.eye, p10.eye);
	DrawLine(mesh.camera.lens, p10.eye, p11.eye);
	DrawLine(mesh.camera.lens, p11.eye, p01.eye);
	DrawLine(mesh.camera.lens, p01.eye, p00.eye);
}

void DEMO_Render(RETRO_Time time)
{
	RETRO_UpdateTerrainRider(time.delta);
	RETRO_ClearDepthBuffer();
	// Far cells first: the lines are not depth tested, so a nearer quad has to paint over them
	RETRO_WalkTerrainMesh(RETRO_BuildTerrainMesh(), DrawCell, NULL, true);
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// Init palette: the sky, and the ramp of the lines from the sky's color
	// through LINE_MIDDLE to LINE_NEAR
	RETRO_SetColor(SKY, RETRO_NIGHTSKY);
	RETRO_CreateGradientPalette(LINE_RAMP0, LINE_RAMP0 + LINE_SHADES / 2, RETRO_NIGHTSKY, LINE_MIDDLE);
	RETRO_CreateGradientPalette(LINE_RAMP0 + LINE_SHADES / 2, LINE_RAMP0 + LINE_SHADES, LINE_MIDDLE, LINE_NEAR);

	RETRO_TerrainDraw.step = LANDSCAPE_STEP;
	RETRO_TerrainDraw.distance = LANDSCAPE_DISTANCE;
	RETRO_Rider.lens.nearplane = LANDSCAPE_NEARPLANE;
	RETRO_Rider.truepitch = true; // PageUp/PageDown tip the view
	RETRO_PlaceTerrainRider(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
