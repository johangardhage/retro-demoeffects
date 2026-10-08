//
// Flat-shaded landscape
//
// The 1024x1024 voxel height and color maps drawn as a wrapping triangle mesh.
// Each mesh quad spans RETRO_TerrainView.step cells on a side and becomes two
// flat-shaded polygons. Their color is the color map at the quad's center and
// their light is the triangle normal, quantized through eight palette-matched
// brightness levels. A depth buffer resolves the mesh without requiring a
// painter's sort.
//
// The sun circles the sky and is drawn where it stands, so the same ground is
// seen lit from every direction in turn and a ridge can be watched brightening
// and falling away again. A face carries one normal over the whole of it, so
// the light lands as facets: gouraudlandscape.cpp takes a normal per vertex
// instead and the same sun crosses it smoothly.
//
// flatlandscape.cpp is the same mesh with the light taken away.
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
#include "lib/retrogfx.h"
#include "lib/retropoly.h"
#include "lib/retroterrain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"
#include "lib/retrovector.h"

#define LANDSCAPE_SHADES 8
// How much light a face turned away from the sun still catches, as a fraction
// of full brightness. See the shade table below for why it is not zero, and
// why it is higher than a lambert term alone would want: this map's palette
// runs out of dark colors before the ramp does
#define LANDSCAPE_AMBIENT 0.55f

// Near enough that a camera flown down onto the ground still sees it
#define LANDSCAPE_NEARPLANE 0.25f

static unsigned char LandscapeShadeTable[RETRO_COLORS][LANDSCAPE_SHADES];

//
// The sun, carried round the sky once every this many seconds
//
// It circles rather than rises. RETRO_TerrainShade maps the band of lambert
// values a height field can actually reach, and the floor of that band is fixed
// by the sun's elevation alone, so holding the elevation holds the whole ramp
// still while the direction turns. The shade table below is matched once
// against that ramp and stays right for every position the sun takes.
//
#define LANDSCAPE_SUNPERIOD 24.0f
// Where the sun stands, as the default direction's height and its reach across
#define LANDSCAPE_SUNHEIGHT 0.62f
#define LANDSCAPE_SUNREACH 0.78f

// The sun itself, drawn where the light comes from. Three entries the color map
// never uses and the shade table can never land on, so nothing on the ground
// can be drawn in them, and three radii in screen pixels: the sun is a
// direction rather than a thing at a distance, so its size on screen does not
// fall off with anything. Three rings and not one because a single disc this
// small is more corner than circle at this resolution.
#define LANDSCAPE_SUNRINGS 3
static const float SunRadius[LANDSCAPE_SUNRINGS] = { 11.0f, 7.0f, 4.0f };
static const unsigned char SunColor[LANDSCAPE_SUNRINGS] = { 247, 248, 249 };

// Toward the sun, carried round each frame; not necessarily unit
static vec3 Sun;

struct WorldVertex {
	vec3 pos;
	RETRO_CameraVertex vertex;
};

// The world position is kept alongside the camera's view of it: the face
// normal below is taken in world space, which the camera's frame no longer is.
static WorldVertex TerrainVertex(float x, float z, const RETRO_Camera *camera)
{
	WorldVertex vertex = {};
	vertex.pos = { x, RETRO_TerrainHeight(x, z), z };
	vertex.vertex.eye = RETRO_ViewPoint(camera, vertex.pos);
	return vertex;
}

//
// The sun, at the point the light arrives from
//
// Sun is a direction and not a place, so there is no distance to
// put the sun at. There does not need to be one: the pinhole divides side and
// height by depth, all three scale together with any distance chosen, and the
// quotients do not move. Handing the direction itself to the same projection
// the ground goes through therefore lands the sun where that ground is lit
// from, and leaves it fixed against the sky as the camera travels.
//
// It is drawn before the mesh and writes no depth, so any hill standing in
// front of it covers it, and the sun sets behind a ridge rather than through it.
//
static void DrawSun(const RETRO_Camera *camera)
{
	vec3 eye = RETRO_ViewDirection(camera, Sun);
	if (eye.z <= 0.0f) return;

	PolygonPoint point = RETRO_ProjectViewPoint(camera->lens, eye);
	for (int ring = 0; ring < LANDSCAPE_SUNRINGS; ring++) {
		RETRO_DrawEllipse(point.pos.x, point.pos.y, SunRadius[ring], SunRadius[ring], SunColor[ring]);
	}
}

static void DrawTriangle(const RETRO_CameraLens &lens, const WorldVertex &a, const WorldVertex &b, const WorldVertex &c, unsigned char basecolor)
{
	RETRO_CameraVertex triangle[3] = { a.vertex, b.vertex, c.vertex };
	PolygonPoint polygon[4];
	int points = RETRO_ClipProjectViewPolygon(lens, triangle, 3, polygon);
	if (points < 3) return;

	// The unnormalized cross product supplies both the face normal and its
	// area. A height field over a regular grid fixes the vertical component at
	// the mesh spacing squared for either winding, since it is built from the x
	// and z spacing alone, so the normal already points upward and the length
	// is never zero.
	vec3 normal = cross(b.pos - a.pos, c.pos - a.pos);
	int shade = RETRO_TerrainShade(normal, Sun, LANDSCAPE_SHADES);

	RETRO_DrawFlatPolygon(polygon, points, LandscapeShadeTable[basecolor][shade]);
}

void DEMO_Render(RETRO_Time time)
{
	RETRO_UpdateTerrainCamera(time.delta);

	// Carry the sun round. Shade divides the length out, so this is a direction
	// and not a brightness: the reach and the height set where it stands, and
	// turning it changes which faces look at it and nothing else.
	float sunangle = fmod(time.total, LANDSCAPE_SUNPERIOD) * (2 * M_PI / LANDSCAPE_SUNPERIOD);
	Sun.x = cosf(sunangle) * LANDSCAPE_SUNREACH;
	Sun.y = LANDSCAPE_SUNHEIGHT;
	Sun.z = sinf(sunangle) * LANDSCAPE_SUNREACH;

	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	int step = mesh.step;

	RETRO_ClearDepthBuffer();
	DrawSun(&mesh.camera);

	for (int z = mesh.minz; z < mesh.maxz; z += step) {
		for (int x = mesh.minx; x < mesh.maxx; x += step) {
			if (!RETRO_TerrainCellVisible(mesh, x, z)) continue;

			WorldVertex p00 = TerrainVertex(x, z, &mesh.camera);
			WorldVertex p10 = TerrainVertex(x + step, z, &mesh.camera);
			WorldVertex p01 = TerrainVertex(x, z + step, &mesh.camera);
			WorldVertex p11 = TerrainVertex(x + step, z + step, &mesh.camera);

			unsigned char color = RETRO_TerrainColor(x + step / 2.0f, z + step / 2.0f);
			DrawTriangle(mesh.camera.lens, p00, p11, p10, color);
			DrawTriangle(mesh.camera.lens, p00, p01, p11, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// Match each darkened source color back to the nearest entry in the map's
	// own palette. Faces can therefore be lit without replacing its terrain
	// colors with a generic material ramp. gouraudlandscape.cpp lights the same
	// map through the same table, one shade per vertex instead of per face.
	//
	// The ambient floor is what makes that affordable. A photographed palette
	// carries a picture's colors and not ramps, so an entry darkened toward
	// black finds nothing near it but the few dark colors the picture happened
	// to contain, and the ground it was lighting drops to one of those: a hole
	// in the hillside rather than a shadow on it. This palette holds two pure
	// blacks - one the picture never uses and one it uses sixteen texels of -
	// so restricting the match to the colors the picture contains does not
	// help. Only the floor does. At 0.40 the darkest shade of a twentieth of
	// the map lands on a black; at this floor none of it does.
	//
	// The floor is therefore set by where the holes stop, not by where the
	// ramps are longest, and those are not the same place. The map uses 184 of
	// the 256 entries. Darkening them the whole way to black leaves 168 with
	// fewer than eight distinct matches, so all but a fiftieth of the map is
	// drawn in ramps that repeat themselves. This floor cuts that to 134 and to
	// under three quarters of the map, where 0.40 would cut it to 93 and to
	// under half. The shorter ramps are what the holes cost.
	RETRO_CreateShadeTable(RETRO_ImagePalette(0), RETRO_COLORS, LANDSCAPE_SHADES, &LandscapeShadeTable[0][0], LANDSCAPE_AMBIENT);

	// Sky, in an entry the color map never uses. The table above matched against
	// the image's own palette, which keeps its copy of entry 0, so this changes
	// only what the entry displays as. The three the sun is drawn in are claimed
	// the same way.
	RETRO_SetColor(0, RETRO_NIGHTSKY);
	RETRO_SetColor(SunColor[0], RETRO_UMBER);
	RETRO_SetColor(SunColor[1], RETRO_FAWN);
	RETRO_SetColor(SunColor[2], RETRO_BLANCHEDALMOND);

	RETRO_TerrainCamera.lens.nearplane = LANDSCAPE_NEARPLANE;
	RETRO_TerrainCamera.truepitch = true; // PageUp/PageDown tip the view
	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
