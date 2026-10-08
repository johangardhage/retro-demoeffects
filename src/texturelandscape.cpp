//
// Texture mapped landscape
//
// The same mesh as flatlandscape.cpp with the color map read per pixel instead
// of per quad: the map is the texture, sampled perspective-correct across every
// triangle, so a quad shows the ground it actually covers rather than the one
// color its center happened to land on. Each quad spans RETRO_TerrainView.step
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
#include "lib/retrovector.h"

// Near enough that a camera flown down onto the ground still sees it
#define LANDSCAPE_NEARPLANE 0.25f

static RETRO_CameraVertex TerrainVertex(float x, float z, const RETRO_Camera *camera)
{
	RETRO_CameraVertex vertex = {};
	vertex.eye = RETRO_ViewPoint(camera, { x, RETRO_TerrainHeight(x, z), z });
	vertex.uv = { x, z };
	return vertex;
}

static void DrawTriangle(const RETRO_CameraLens &lens, const RETRO_CameraVertex &a, const RETRO_CameraVertex &b, const RETRO_CameraVertex &c)
{
	RETRO_CameraVertex triangle[3] = { a, b, c };
	PolygonPoint polygon[4];
	int points = RETRO_ClipProjectViewPolygon(lens, triangle, 3, polygon);
	if (points < 3) return;

	RETRO_DrawTexMapPolygon(polygon, points, RETRO_Terrain.colormap, RETRO_Terrain.width, RETRO_Terrain.height, RETRO_Terrain.wrap);
}

void DEMO_Render(RETRO_Time time)
{
	RETRO_UpdateTerrainCamera(time.delta);
	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	int step = mesh.step;

	RETRO_ClearDepthBuffer();
	for (int z = mesh.minz; z < mesh.maxz; z += step) {
		for (int x = mesh.minx; x < mesh.maxx; x += step) {
			if (!RETRO_TerrainCellVisible(mesh, x, z)) continue;

			RETRO_CameraVertex p00 = TerrainVertex(x, z, &mesh.camera);
			RETRO_CameraVertex p10 = TerrainVertex(x + step, z, &mesh.camera);
			RETRO_CameraVertex p01 = TerrainVertex(x, z + step, &mesh.camera);
			RETRO_CameraVertex p11 = TerrainVertex(x + step, z + step, &mesh.camera);

			DrawTriangle(mesh.camera.lens, p00, p11, p10);
			DrawTriangle(mesh.camera.lens, p00, p01, p11);
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

	RETRO_TerrainCamera.lens.nearplane = LANDSCAPE_NEARPLANE;
	RETRO_TerrainCamera.truepitch = true; // PageUp/PageDown tip the view
	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
