//
// Flight sim
//
// An aircraft flown over the 1024x1024 voxel height and color maps, drawn as
// the flat-shaded triangle mesh of flatshadedlandscape.cpp. The wrapping
// camera there has only a heading; an aircraft also pitches and rolls, so the
// view here is a free frame of right, up and forward vectors in world space,
// turned about its own axes. A world point lands in that frame as three dot
// products, and the near-plane clip and pinhole of the terrain library take
// it from there. The two focal lengths are equal and the principal point sits
// at the middle of the screen, so a roll turns the picture about its center
// without stretching it.
//
// The flight model is arcade:
//
//   - The stick turns the frame: Left/Right roll at a fixed rate, and Up/Down
//     pitch the nose down and up at a fixed rate, or slower where the wings
//     cannot push or pull that hard at the speed.
//   - A bank turns the heading. The wings pull a fixed load factor, and the
//     part of it tipped sideways by the bank turns the heading about the
//     world's upright axis at that acceleration over the speed, so a turn
//     tightens as the aircraft slows, fades out as the nose comes vertical
//     and reverses when inverted. Below the stall speed it tightens no more.
//   - Speed is drawn toward the throttle setting, gained in a dive and lost
//     in a climb.
//   - Lift is lost as the wings tip away from level, and below the stall
//     speed, as a sink straight down.
//   - The aircraft cannot fly into the ground: it is held a clearance above
//     the drawn mesh, and its nose is brought level when it touches.
//
// Each mesh cell is tested against the view frustum by its four corners, the
// whole cell rejected only if every corner is outside the same plane. Cells
// beyond the draw distance are dropped by their center, which rounds the far
// edge of the ground against the sky.
//
// Up/Down pitch and Left/Right roll. A/D work the rudder and W/S the
// throttle.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retroterrain.h"
#include "lib/retrocamera.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"
#include "lib/retrovector.h"
#include "lib/retromatrix.h"

#define LANDSCAPE_SHADES 8
#define LANDSCAPE_AMBIENT 0.55f
#define LANDSCAPE_NEARPLANE 0.5f
#define LANDSCAPE_STEP 6

// The ground fades into the haze along the horizon over this many levels,
// starting this far out and gone into it at the draw distance
#define FOG_LEVELS 16
#define FOG_START 250.0f

// The sky, from the haze at the horizon to the blue overhead, in held palette
// entries from 0. The fog fades into the first of them.
#define SKY_BANDS 32
#define SKY_HORIZON RETRO_Palette{ 195, 226, 245 }
#define SKY_ZENITH RETRO_STEELBLUE

#define FLIGHT_PITCHRATE 1.2f		// Radians per second at full stick, at most
#define FLIGHT_ROLLRATE 2.5f
#define FLIGHT_RUDDERRATE 0.3f
#define FLIGHT_TURNLOAD 2.0f		// Load factor the wings pull in a bank, in g
#define FLIGHT_PULLLOAD 6.0f		// and the further load at full stick back,
#define FLIGHT_PUSHLOAD 4.0f		// and the load taken off at full stick forward
#define FLIGHT_MINTHROTTLE 60.0f	// Cells per second
#define FLIGHT_MAXTHROTTLE 240.0f
#define FLIGHT_THROTTLERATE 60.0f	// Throttle change per second the key is held
#define FLIGHT_SPOOL 3.0f			// Seconds the speed takes to close most of the gap to the throttle
#define FLIGHT_GRAVITY 50.0f		// One g: speed gained per second in a vertical dive
#define FLIGHT_MINSPEED 20.0f
#define FLIGHT_STALLSPEED 50.0f
#define FLIGHT_SINK 12.0f			// Sink per second with the wings vertical
#define FLIGHT_STALLSINK 40.0f		// Further sink per second at no speed at all
#define FLIGHT_CLEARANCE 8.0f		// Closest the eye comes to the drawn ground
#define FLIGHT_CEILING 600.0f		// Highest the eye may climb

static RETRO_ColorHistogram Histogram;
static unsigned char LandscapeTable[RETRO_COLORS * LANDSCAPE_SHADES * FOG_LEVELS];

//
// A map color lit and fogged: shade from the ambient floor to full light, then
// the fog level, the table's one tint, toward the haze. The floor is how much
// light a face turned away from the sun still catches.
//
static RETRO_Palette Light(RETRO_Palette color, float shade, const float *tint)
{
	return mix(RETRO_ShadeColor(color, mix(LANDSCAPE_AMBIENT, 1.0f, shade)), SKY_HORIZON, tint[0]);
}

struct Aircraft {
	vec3 right;
	vec3 up;
	vec3 forward;
	float speed;
	float throttle;
};

// Level, heading along decreasing z like the wrapping camera at heading zero
static Aircraft Plane = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, 120.0f, 120.0f };
static RETRO_Camera Camera;

struct WorldVertex {
	vec3 pos;
	RETRO_CameraVertex vertex;
};

// The whole frame turned about one axis. With the frame's own axes, positive
// angles are nose up about right, right wing down about forward and nose left
// about up.
static void TurnPlane(vec3 axis, float angle)
{
	mat3 turn = rotate(angle, axis);
	Plane.right = turn * Plane.right;
	Plane.up = turn * Plane.up;
	Plane.forward = turn * Plane.forward;
}

// The ground as drawn: the mesh triangle under (x, z), split along the same
// diagonal the mesh is, rather than the map at full resolution, which a mesh
// several cells across cuts corners off
static float MeshHeight(float x, float z)
{
	float step = LANDSCAPE_STEP;
	float x0 = floorf(x / step) * step;
	float z0 = floorf(z / step) * step;
	float fx = (x - x0) / step;
	float fz = (z - z0) / step;
	float h00 = RETRO_TerrainHeight(x0, z0);
	float h10 = RETRO_TerrainHeight(x0 + step, z0);
	float h01 = RETRO_TerrainHeight(x0, z0 + step);
	float h11 = RETRO_TerrainHeight(x0 + step, z0 + step);
	if (fx >= fz) {
		return h00 + fx * (h10 - h00) + fz * (h11 - h10);
	} else {
		return h00 + fz * (h01 - h00) + fx * (h11 - h01);
	}
}

static void UpdateFlight(float timestep)
{
	vec3 worldup = { 0, 1, 0 };

	// The stick pitches the nose as fast as the wings can pull it round at the
	// speed, the load over the speed, but no faster than full stick moves it.
	// The wings push less hard than they pull. Below the stall speed they pull
	// no harder.
	float airspeed = MAX(Plane.speed, FLIGHT_STALLSPEED);
	float pullrate = MIN(FLIGHT_PITCHRATE, FLIGHT_PULLLOAD * FLIGHT_GRAVITY / airspeed);
	float pushrate = MIN(FLIGHT_PITCHRATE, FLIGHT_PUSHLOAD * FLIGHT_GRAVITY / airspeed);
	if (RETRO_KeyState(SDL_SCANCODE_UP)) TurnPlane(Plane.right, -pushrate * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_DOWN)) TurnPlane(Plane.right, pullrate * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) TurnPlane(Plane.forward, -FLIGHT_ROLLRATE * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) TurnPlane(Plane.forward, FLIGHT_ROLLRATE * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_A)) TurnPlane(Plane.up, FLIGHT_RUDDERRATE * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_D)) TurnPlane(Plane.up, -FLIGHT_RUDDERRATE * timestep);
	if (RETRO_KeyState(SDL_SCANCODE_W)) Plane.throttle += FLIGHT_THROTTLERATE * timestep;
	if (RETRO_KeyState(SDL_SCANCODE_S)) Plane.throttle -= FLIGHT_THROTTLERATE * timestep;
	Plane.throttle = clamp(Plane.throttle, FLIGHT_MINTHROTTLE, FLIGHT_MAXTHROTTLE);

	// Banked right, the right wing points down and right.y is negative, and
	// -right.y of the wings' pull is sideways. A sideways acceleration over the
	// speed is the rate of turn. A positive turn about world up swings the nose
	// left, so the bank turns the heading by right.y itself.
	TurnPlane(worldup, FLIGHT_TURNLOAD * FLIGHT_GRAVITY * Plane.right.y / airspeed * timestep);

	// The turns are exact, but rounding still creeps in over thousands of them
	Plane.forward = normalize(Plane.forward);
	Plane.right = normalize(cross(Plane.forward, Plane.up));
	Plane.up = cross(Plane.right, Plane.forward);

	Plane.speed = mix(Plane.speed, Plane.throttle, 1.0f - expf(-timestep / FLIGHT_SPOOL));
	Plane.speed -= Plane.forward.y * FLIGHT_GRAVITY * timestep;
	Plane.speed = MAX(Plane.speed, FLIGHT_MINSPEED);

	// Level wings lose nothing, vertical ones FLIGHT_SINK, inverted ones twice it
	float sink = (1.0f - Plane.up.y) * FLIGHT_SINK;
	if (Plane.speed < FLIGHT_STALLSPEED) sink += (1.0f - Plane.speed / FLIGHT_STALLSPEED) * FLIGHT_STALLSINK;

	vec3 velocity = Plane.forward * Plane.speed - worldup * sink;
	RETRO_TerrainCamera.x += velocity.x * timestep;
	RETRO_TerrainCamera.z += velocity.z * timestep;
	RETRO_TerrainCamera.height = MIN(RETRO_TerrainCamera.height + velocity.y * timestep, FLIGHT_CEILING);
	RETRO_WrapTerrainCamera();

	// On the ground. The nose is turned about the horizontal axis across it,
	// cross(forward, up), which lifts it toward the sky whichever way up the
	// aircraft is, by exactly the angle it was pointing down.
	float floor = MeshHeight(RETRO_TerrainCamera.x, RETRO_TerrainCamera.z) + FLIGHT_CLEARANCE;
	if (RETRO_TerrainCamera.height < floor) {
		RETRO_TerrainCamera.height = floor;
		vec3 across = cross(Plane.forward, worldup);
		if (Plane.forward.y < 0 && length(across) > 0) {
			TurnPlane(normalize(across), -asinf(Plane.forward.y));
		}
	}
}

// The view from the cockpit: the aircraft's frame, with its up turned into
// the camera's down, through the terrain's lens
static void PlaceCamera(void)
{
	Camera.pos = { RETRO_TerrainCamera.x, RETRO_TerrainCamera.height, RETRO_TerrainCamera.z };
	Camera.right = Plane.right;
	Camera.down = -Plane.up;
	Camera.forward = Plane.forward;
	Camera.lens = RETRO_TerrainLens;
}

// A world point in the aircraft's frame
static WorldVertex TerrainVertex(float x, float z)
{
	WorldVertex vertex = {};
	vertex.pos = { x, RETRO_TerrainHeight(x, z), z };
	vertex.vertex.eye = RETRO_ViewPoint(&Camera, vertex.pos);
	return vertex;
}

// Whether all four corners are outside one plane of the frustum. The side
// planes are widened a little past the screen edges for the same reason the
// terrain library's cull wedge is.
static bool CellOutside(const WorldVertex *corner)
{
	float slopex = (RETRO_WIDTH * 0.5f) / RETRO_TerrainLens.focalx * 1.05f;
	float slopey = (RETRO_HEIGHT * 0.5f) / RETRO_TerrainLens.focaly * 1.05f;
	int behind = 0, left = 0, right = 0, above = 0, below = 0;
	for (int i = 0; i < 4; i++) {
		const vec3 &eye = corner[i].vertex.eye;
		if (eye.z < RETRO_TerrainLens.nearplane) behind++;
		if (eye.x < -eye.z * slopex) left++;
		if (eye.x > eye.z * slopex) right++;
		if (eye.y < -eye.z * slopey) above++;
		if (eye.y > eye.z * slopey) below++;
	}
	return behind == 4 || left == 4 || right == 4 || above == 4 || below == 4;
}

//
// The sky behind everything, by the height of each pixel's ray
//
// The ray through a pixel's center is the camera's view ray. Its height over
// its length is the sine of how far above the horizon it looks, so the
// gradient follows the horizon through any roll or pitch. The square root
// lifts the haze quickly off the horizon into the blue. A ray below the
// horizon looks at ground past the draw distance, which the fog has already
// faded into the haze, so it is drawn in the haze too.
//
static void DrawSky(void)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			vec3 ray = RETRO_ViewRay(&Camera, { x + 0.5f, y + 0.5f });
			float elevation = MAX(ray.y / length(ray), 0.0f);
			int band = MIN((int)(sqrtf(elevation) * (SKY_BANDS - 1) + RETRO_DitherThreshold(x, y)), SKY_BANDS - 1);
			buffer[y * RETRO_WIDTH + x] = band;
		}
	}
}

// A face is fogged by the distance to its center, so it takes one fog level as
// it takes one shade. The distance is taken across the ground, as the draw
// distance is, so the edge is gone into the haze at any altitude while the
// ground below a high aircraft stays clear.
static void DrawTriangle(const WorldVertex &a, const WorldVertex &b, const WorldVertex &c, unsigned char basecolor)
{
	RETRO_CameraVertex triangle[3] = { a.vertex, b.vertex, c.vertex };
	PolygonPoint polygon[4];
	int points = RETRO_ClipProjectViewPolygon(RETRO_TerrainLens, triangle, 3, polygon);
	if (points < 3) return;

	vec3 normal = cross(b.pos - a.pos, c.pos - a.pos);
	int shade = RETRO_TerrainShade(normal, LANDSCAPE_SHADES);

	vec3 center = (a.pos + b.pos + c.pos) * (1.0f / 3.0f);
	float distance = hypotf(center.x - RETRO_TerrainCamera.x, center.z - RETRO_TerrainCamera.z);
	float fog = clamp((distance - FOG_START) / (RETRO_TerrainView.distance - FOG_START), 0.0f, 1.0f);
	int level = (int)(fog * (FOG_LEVELS - 1) + 0.5f);

	RETRO_DrawFlatPolygon(polygon, points, LandscapeTable[(basecolor * LANDSCAPE_SHADES + shade) * FOG_LEVELS + level]);
}

void DEMO_Render(RETRO_Time time)
{
	UpdateFlight(time.delta);
	PlaceCamera();

	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	int step = mesh.step;

	RETRO_ClearDepthBuffer();
	DrawSky();

	for (int z = mesh.minz; z < mesh.maxz; z += step) {
		for (int x = mesh.minx; x < mesh.maxx; x += step) {
			float centerx = x + step / 2.0f - RETRO_TerrainCamera.x;
			float centerz = z + step / 2.0f - RETRO_TerrainCamera.z;
			if (centerx * centerx + centerz * centerz > mesh.distance2) continue;

			WorldVertex corner[4] = {
				TerrainVertex(x, z),
				TerrainVertex(x + step, z),
				TerrainVertex(x, z + step),
				TerrainVertex(x + step, z + step),
			};
			if (CellOutside(corner)) continue;

			unsigned char color = RETRO_TerrainColor(x + step / 2.0f, z + step / 2.0f);
			DrawTriangle(corner[0], corner[3], corner[1], color);
			DrawTriangle(corner[0], corner[2], corner[3], color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// The light, low in the sky ahead at takeoff
	RETRO_TerrainLight = { -0.50f, 0.45f, -0.75f };

	// The palette holds the sky, and is fitted around it to the
	// map's colors at every shade and fog level, each weighed by how much of
	// the map has it. The table then lights and fogs the map's colors onto it.
	// Fogged all the way, a color is the haze itself, which is the first held
	// entry, so the far ground meets the sky without a seam.
	RETRO_Palette palette[RETRO_COLORS];
	RETRO_Palette held[SKY_BANDS];
	for (int band = 0; band < SKY_BANDS; band++) {
		held[band] = mix(SKY_HORIZON, SKY_ZENITH, RETRO_ShadeTableLevel(band, SKY_BANDS));
	}
	float lightweight[LANDSCAPE_SHADES * FOG_LEVELS];
	for (float &weight : lightweight) {
		weight = 1;
	}
	RETRO_ShadeTable shadetable = { LandscapeTable, RETRO_COLORS, LANDSCAPE_SHADES, { FOG_LEVELS } };
	RETRO_AddShadeTableColors(&Histogram, 0, shadetable, lightweight, Light);
	RETRO_CreateHistogramPalette(&Histogram, palette, held, SKY_BANDS);
	RETRO_SetPalette(palette);
	RETRO_CreateShadeTable(RETRO_ImagePalette(0), palette, shadetable, Light);

	RETRO_TerrainView.step = LANDSCAPE_STEP;
	RETRO_TerrainLens.nearplane = LANDSCAPE_NEARPLANE;
	RETRO_TerrainLens.focalx = RETRO_WIDTH * 0.5f;
	RETRO_TerrainLens.focaly = RETRO_WIDTH * 0.5f;
	RETRO_TerrainLens.center.y = RETRO_HEIGHT * 0.5f;

	RETRO_TerrainCamera.x = RETRO_Terrain.width * 0.5f;
	RETRO_TerrainCamera.z = (float)RETRO_TERRAIN_DISTANCE;
	RETRO_TerrainCamera.height = MeshHeight(RETRO_TerrainCamera.x, RETRO_TerrainCamera.z) + 80.0f;
}
