//
// Shadow 2
//
// A lit, textured landscape to drive over, an object orbiting above it, and
// four lights - ambient, a sun, and a green and a red point light circling
// the world - with a cube marking each point light. Each point light throws
// a shadow of the object onto the ground, and either can be lowered and
// raised: the nearer it comes down to the object, the larger its shadow.
// Either shadow can instead be cast from straight above the object, at its
// light's altitude.
//
// The world is measured in units of its own, 4000 across the 40 by 40 height
// map; WORLD() takes those to map cells.
//
// Two point lights have a hue, so a vertex's light is three numbers: a
// neutral level, and how much further red and green reach. All three are
// interpolated across a face and looked up, with the texel, in a table per
// texture that takes the texture's own palette onto the screen's:
//
//   table[t][n][r][g] = nearest(texture[t] · (min(n + r, 1), min(n + g, 1), n))
//
// Each shadow is the convex outline of the object's corners as its light
// sees them, each corner of it followed along its ray from the light to where
// it meets the ground and lifted just above it. On flat ground, an object of
// radius r at height h under a light at height l throws a shadow of radius
// r * l / (l - h), stretched away from the light by how far to the side of
// it the object is. It falls off the map when the light comes down near the
// object's height, and tilts and bends with a slope rather than floating over
// it. It is depth tested but writes no depth, and darkens what is under it
// through a table, so where the two shadows cross it is darker still. With
// a point light out, or down level with any of the object, there is none
// from it.
//
// The screen's palette is fitted as the demo starts, to the colors those
// tables will ask for: each texture's colors, as often as its texels have
// them, under each light as often as the ground sees it with the point
// lights at points round their paths, and the ground's again in shadow.
// Changing a light changes the palette with it.
//
// Faces are cut at the near plane in the camera's frame before they are
// projected, so one reaching from behind the camera leaves no hole.
//
// Up/Down drive, Left/Right turn. L all lighting, A ambient, I sun, P both
// point lights, G and R the green and the red one, O next object, 1 and 2
// lower and raise the green light, 3 and 4 the red one, C cast from above or
// from the lights, H help.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropoly.h"
#include "lib/retromodel.h"
#include "lib/retromath.h"
#include "lib/retroterrain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"
#include "lib/retrofont.h"
#include "lib/retrovector.h"

// The terrain is 4000 world units across 40 samples
#define WORLD(units) ((units) * (40 - 1) / 4000.0f)
#define TO_WORLD(cells) ((cells) * 4000.0f / (40 - 1))

#define TERRAIN_SCALE WORLD(700.0f / 255) // map cells of height per stored byte

#define AMBIENT_LIGHT (100 / 256.0f)
#define INFINITE_LIGHT (100 / 256.0f)
#define POINT_LIGHT (255 / 256.0f)
#define POINT_LIGHT_RED_KL 0.002f // attenuation per world unit
#define POINT_LIGHT_GREEN_KL 0.001f
#define POINT_LIGHT_RATE radians(30) // radians a second round the world
#define POINT_LIGHT_RED_ORBIT WORLD(200) // radius about the world's center, circled twice as fast as green the other way
#define POINT_LIGHT_GREEN_ORBIT WORLD(500)
#define POINT_LIGHT_ALTITUDE WORLD(500) // where both start
#define POINT_LIGHT_RAISE WORLD(300) // a second, while 1 to 4 is held
#define POINT_LIGHT_TOP WORLD(2000) // the highest 1 to 4 raise a light, and the ground's base the lowest
#define LIGHT_LEVELS 32 // neutral light, in the tables
#define TINT_LEVELS 8 // and each point light's color beyond it

#define OBJECT_SCALE WORLD(40)
#define OBJECT_ORBIT WORLD(150) // radius of the orbit about the world's center
#define OBJECT_ALTITUDE WORLD(200) // mean height of the orbit
#define OBJECT_BOB WORLD(75) // and how far it rises and falls, once a lap
#define OBJECT_RATE radians(30) // radians a second round the orbit
#define LIGHT_OBJECT_SCALE WORLD(10)

#define SHADOW_LIFT WORLD(25) // above the ground it lies on
#define SHADOW_MAX_CORNERS 30 // on a shadow's outline; a model that needs more casts none
#define SHADOW_STEP WORLD(10) // along a ray, looking for the ground
#define SHADOW_LIGHT 0.5f // fraction of the light left in the shadow

#define PALETTE_TURNS 8 // points round the point lights' paths the palette is fitted at
#define OBJECT_WEIGHT (1 / 8.0f) // an object texture's say in the palette, beside the ground's
#define SHADOW_WEIGHT (1 / 4.0f) // and the ground's in shadow

#define TEXTURE_SIZE 256 // every texture, on a side

enum { ASSET_TERRAIN, ASSET_EARTH, ASSET_CUBE, ASSET_HEIGHTMAP };
#define TEXTURES (ASSET_HEIGHTMAP - ASSET_TERRAIN)
enum { MODEL_SPHERE, MODEL_CUBE, MODELS };
enum { LIGHT_RED, LIGHT_GREEN, POINT_LIGHTS, LIGHT_SUN = POINT_LIGHTS, LIGHTS };

struct Object {
	int model;
	int texture;
};

static const Object Objects[] = {
	{ MODEL_SPHERE, ASSET_EARTH },
	{ MODEL_CUBE, ASSET_CUBE }
};
#define OBJECTS (int)(sizeof(Objects) / sizeof(Objects[0]))

static unsigned char LightTableData[TEXTURES][RETRO_COLORS][LIGHT_LEVELS][TINT_LEVELS][TINT_LEVELS];
static unsigned char ShadowTable[RETRO_COLORS];
static unsigned char ColorBlack, ColorTextGreen, ColorWhite, ColorCubeGreen, ColorCubeRed, ColorSky, ColorGround;
static Model3D *Models[MODELS];

// The screen's palette, and what it is fitted to
static RETRO_Palette ScreenPalette[RETRO_COLORS];
static RETRO_ColorHistogram Histogram;
static float LightWeight[LIGHT_LEVELS * TINT_LEVELS * TINT_LEVELS]; // how often the ground sees each light, laid out as the tables

// The colors drawn besides the textures, held in the palette exactly
static const RETRO_Palette CubeGreen = { 0, 255, 10 }, CubeRed = { 255, 0, 0 }, Sky = { 50, 50, 200 }, Ground = { 25, 50, 110 };
static const RETRO_Palette Held[] = { RETRO_BLACK, RETRO_GREEN, RETRO_WHITE, CubeGreen, CubeRed, Sky, Ground };
#define HELD (int)(sizeof(Held) / sizeof(Held[0]))

// The point lights, placed each frame, and the sun, the infinite light,
// toward it. All are in the world, in map cells, where a point light d cells
// away dims as POINT_LIGHT / (kl TO_WORLD(d)). The point lights shine their
// own channels and the sun white, over the ambient light. The keys switch
// them on and off in the rig
static RETRO_Lighting Lights = { LIGHTS, {
	{ {}, POINT_LIGHT, true, {}, 0, POINT_LIGHT_RED_KL * TO_WORLD(1), { 1, 0, 0 } },
	{ {}, POINT_LIGHT, true, {}, 0, POINT_LIGHT_GREEN_KL * TO_WORLD(1), { 0, 1, 0 } },
	{ normalize(vec3{ 1, 1, -1 }), INFINITE_LIGHT },
}, AMBIENT_LIGHT };
static_assert(LIGHTS <= RETRO_MAX_LIGHTS, "The point lights and the sun share one rig");
static const RETRO_Lighting Darkness = {}; // what everything is lit by with the lighting off
static float PointLightAltitude[POINT_LIGHTS] = { POINT_LIGHT_ALTITUDE, POINT_LIGHT_ALTITUDE };

// The switches
static bool Lighting = true;
static bool Help = true;
static bool CastFromAbove = false; // or from the point lights

// The camera's frame, taken once a frame
static RETRO_Camera View;

// A vertex at p facing n, lit. Only the ambient light and the sun have any
// blue in them, and they are white, as the light tables need
static vec3 LightVertex(vec3 p, vec3 n)
{
	return RETRO_ColorLambert(Lighting ? Lights : Darkness, p, n);
}

// A texture's light table, from its own colors to the screen's
static RETRO_ShadeTable LightTable(int texture)
{
	return { &LightTableData[texture - ASSET_TERRAIN][0][0][0][0], RETRO_COLORS, LIGHT_LEVELS, { TINT_LEVELS, TINT_LEVELS } };
}

// The ground at sample x, z, lit
static RETRO_CameraVertex TerrainVertex(int x, int z)
{
	vec3 p = { (float)x, RETRO_TerrainHeight(x, z), (float)z };
	vec3 n = normalize(RETRO_TerrainNormal(p.x, p.z));

	// The texture is stretched once over the patch
	vec2 uv = { p.x * (TEXTURE_SIZE - 1) / (RETRO_Terrain.width - 1), p.z * (TEXTURE_SIZE - 1) / (RETRO_Terrain.height - 1) };
	return RETRO_TerrainTintVertex(p, uv, LightVertex(p, n), LightTable(ASSET_TERRAIN), &View);
}

// Where the ray from light through p meets the ground
static bool CastFrom(vec3 light, vec3 p, vec3 &ground)
{
	vec3 ray = p - light;
	float distance = length(ray);
	if (distance <= 0) return false;
	return RETRO_TerrainRayHit(p, ray / distance, SHADOW_STEP, ground);
}

// The shadow of a model at a position and scale, cast by a point light
static void DrawShadow(const Model3D *model, vec3 position, float scale, vec3 light)
{
	vec3 center;
	if (!CastFrom(light, position, center)) return;
	int x = (int)floorf(center.x);
	int z = (int)floorf(center.z);
	if (x < 0 || z < 0 || x >= RETRO_Terrain.width - 1 || z >= RETRO_Terrain.height - 1) return;

	// Two directions across the ray to the object's center, taken off
	// whichever axis is not along it, and the model's corners as the light
	// sees them: across the ray over the distance along it. A corner level
	// with the light or behind it has no ray to the ground
	vec3 ray = normalize(position - light);
	vec3 axis = fabsf(ray.y) < 0.99f ? vec3{ 0, 1, 0 } : vec3{ 0, 0, 1 };
	vec3 across = normalize(cross(ray, axis));
	vec3 up = cross(across, ray);
	vec2 seen[RETRO_MAX_VERTICES];
	for (int i = 0; i < model->vertices; i++) {
		vec3 v = position + model->vertex[i].pos * scale - light;
		float depth = dot(v, ray);
		if (depth <= 0) return;
		seen[i] = { dot(v, across) / depth, dot(v, up) / depth };
	}
	int outline[SHADOW_MAX_CORNERS];
	int corners = RETRO_ConvexOutline(seen, model->vertices, outline, SHADOW_MAX_CORNERS);
	if (corners < 3) return;

	RETRO_CameraVertex shadow[SHADOW_MAX_CORNERS];
	for (int i = 0; i < corners; i++) {
		vec3 corner = position + model->vertex[outline[i]].pos * scale;
		vec3 p;
		if (!CastFrom(light, corner, p)) return;
		shadow[i] = RETRO_ViewVertex(&View, { p.x, p.y + SHADOW_LIFT, p.z });
	}
	// Each corner lands at its own height, so the outline is not flat and can
	// turn concave on screen, where a fan from one of its corners reaches
	// past it. Fanned from the center's shadow instead, the triangles stay
	// inside the outline; each is clipped on its own, so the near plane
	// cannot move the fan off that center
	RETRO_CameraVertex middle = RETRO_ViewVertex(&View, { center.x, center.y + SHADOW_LIFT, center.z });
	for (int i = 0; i < corners; i++) {
		RETRO_CameraVertex triangle[3] = { middle, shadow[i], shadow[(i + 1) % corners] };
		PolygonPoint polygon[4];
		int points = RETRO_ClipProjectViewPolygon(View.lens, triangle, 3, polygon);
		// One pass for the whole fan, including overlapping triangles.
		RETRO_DrawRemapPolygon(polygon, points, ShadowTable, i == 0);
	}
}

static void DrawText(void)
{
	char text[256];

	RETRO_PutString("Press ESC to exit. Press <H> for Help.", 0, 0, ColorTextGreen);
	if (Help) {
		RETRO_PutString("<L> Toggle all lighting\n"
						"<A> Toggle ambient light source\n"
						"<I> Toggle infinite light source\n"
						"<P> Toggle both point lights\n"
						"<G>/<R> Toggle green/red light\n"
						"<O> Select different objects\n"
						"<1>/<2> Lower/raise green light\n"
						"<3>/<4> Lower/raise red light\n"
						"<C> Cast from above/point lights\n"
						"<H> Toggle Help\n"
						"<Q> Exit demo", 0, 14, ColorWhite);
	}

	// CELL is the cell the camera is over
	vec3 center = RETRO_TerrainCenter();
	// Each line within the screen's 40 columns
	snprintf(text, sizeof(text), "CAM [%5.0f,%5.0f,%5.0f] CELL [%d, %d]\nLighting [%s]: Amb=%d Inf=%d G=%d R=%d\nGreen y=%.0f Red y=%.0f\nShadow cast from [%s]",
			 TO_WORLD(RETRO_TerrainCamera.x - center.x), TO_WORLD(RETRO_TerrainCamera.height), TO_WORLD(RETRO_TerrainCamera.z - center.z), (int)floorf(RETRO_TerrainCamera.x), (int)floorf(RETRO_TerrainCamera.z),
			 Lighting ? "ON" : "OFF", Lights.ambient > 0, Lights.light[LIGHT_SUN].on, Lights.light[LIGHT_GREEN].on, Lights.light[LIGHT_RED].on, TO_WORLD(PointLightAltitude[LIGHT_GREEN]), TO_WORLD(PointLightAltitude[LIGHT_RED]),
			 CastFromAbove ? "ABOVE" : "POINT LIGHTS");
	RETRO_PutString(text, 0, RETRO_HEIGHT - 37, ColorTextGreen);
}

// The point lights on their paths about the world's center
static void PlacePointLights(double time)
{
	vec3 center = RETRO_TerrainCenter();
	float angle = fmod(time * POINT_LIGHT_RATE, 2 * M_PI);
	Lights.light[LIGHT_GREEN].position = { center.x - POINT_LIGHT_GREEN_ORBIT * cosf(angle), PointLightAltitude[LIGHT_GREEN], center.z + POINT_LIGHT_GREEN_ORBIT * sinf(angle) };
	Lights.light[LIGHT_RED].position = { center.x - POINT_LIGHT_RED_ORBIT * cosf(-2 * angle), PointLightAltitude[LIGHT_RED], center.z + POINT_LIGHT_RED_ORBIT * sinf(-2 * angle) };
}

//
// The screen's palette, fitted to the colors the light tables will ask for
//
static void FitScreenPalette(void)
{
	// How often the ground sees each light the tables hold, rounded to its
	// levels as the ground's corners round it, with the point lights at
	// points round their paths
	for (int turn = 0; turn < PALETTE_TURNS; turn++) {
		PlacePointLights(turn * 2 * M_PI / PALETTE_TURNS / POINT_LIGHT_RATE);
		for (int z = 0; z < RETRO_Terrain.height; z++) {
			for (int x = 0; x < RETRO_Terrain.width; x++) {
				vec3 p = { (float)x, RETRO_TerrainHeight(x, z), (float)z };
				vec3 light = LightVertex(p, normalize(RETRO_TerrainNormal(p.x, p.z)));
				LightWeight[RETRO_TintEntry(LightTable(ASSET_TERRAIN), light)]++;
			}
		}
	}

	// Each texture's colors, as often as its texels have them, under those
	// lights: the ground's, then the objects' with less say, since they
	// cover less of the screen, and the ground's again in shadow
	for (int texture = ASSET_TERRAIN; texture < ASSET_TERRAIN + TEXTURES; texture++) {
		if (texture == ASSET_TERRAIN) {
			RETRO_AddShadeTableColors(&Histogram, texture, LightTable(texture), LightWeight, RETRO_TintColor);
			RETRO_AddShadeTableColors(&Histogram, texture, LightTable(texture), LightWeight, RETRO_TintColor, SHADOW_WEIGHT, SHADOW_LIGHT);
		} else {
			RETRO_AddShadeTableColors(&Histogram, texture, LightTable(texture), LightWeight, RETRO_TintColor, OBJECT_WEIGHT);
		}
	}

	RETRO_CreateHistogramPalette(&Histogram, ScreenPalette, Held, HELD);
	RETRO_SetPalette(ScreenPalette);
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	RETRO_UpdateTerrainVehicle(time.delta);
}

void DEMO_Render(RETRO_Time time)
{
	static int currentobject;

	if (RETRO_KeyPressed(SDL_SCANCODE_L)) Lighting = !Lighting;
	if (RETRO_KeyPressed(SDL_SCANCODE_A)) Lights.ambient = Lights.ambient > 0 ? 0 : AMBIENT_LIGHT;
	if (RETRO_KeyPressed(SDL_SCANCODE_I)) Lights.light[LIGHT_SUN].on = !Lights.light[LIGHT_SUN].on;
	if (RETRO_KeyPressed(SDL_SCANCODE_P)) {
		// Both on unless both already are
		bool on = !(Lights.light[LIGHT_GREEN].on && Lights.light[LIGHT_RED].on);
		Lights.light[LIGHT_GREEN].on = Lights.light[LIGHT_RED].on = on;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_G)) Lights.light[LIGHT_GREEN].on = !Lights.light[LIGHT_GREEN].on;
	if (RETRO_KeyPressed(SDL_SCANCODE_R)) Lights.light[LIGHT_RED].on = !Lights.light[LIGHT_RED].on;
	if (RETRO_KeyPressed(SDL_SCANCODE_H)) Help = !Help;
	if (RETRO_KeyPressed(SDL_SCANCODE_O)) currentobject = (currentobject + 1) % OBJECTS;
	float raise = POINT_LIGHT_RAISE * time.delta;
	if (RETRO_KeyState(SDL_SCANCODE_1)) PointLightAltitude[LIGHT_GREEN] -= raise;
	if (RETRO_KeyState(SDL_SCANCODE_2)) PointLightAltitude[LIGHT_GREEN] += raise;
	if (RETRO_KeyState(SDL_SCANCODE_3)) PointLightAltitude[LIGHT_RED] -= raise;
	if (RETRO_KeyState(SDL_SCANCODE_4)) PointLightAltitude[LIGHT_RED] += raise;
	for (float &altitude : PointLightAltitude) {
		altitude = clamp(altitude, 0.0f, POINT_LIGHT_TOP);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_C)) CastFromAbove = !CastFromAbove;

	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	View = mesh.camera;

	PlacePointLights(time.total);
	vec3 center = RETRO_TerrainCenter();

	// The object on its orbit, bobbing once a lap
	float orbit = fmod(time.total * OBJECT_RATE, 2 * M_PI);
	vec3 object = { center.x - OBJECT_ORBIT * cosf(orbit), OBJECT_ALTITUDE + OBJECT_BOB * sinf(orbit), center.z + OBJECT_ORBIT * sinf(orbit) };
	const Object &selected = Objects[currentobject];
	const Model3D *model = Models[selected.model];

	// The sky, and the ground color below the camera's horizon for wherever
	// the landscape does not reach, so it follows the vehicle's pitch
	RETRO_ClearDepthBuffer();
	RETRO_DrawHorizon(&View, { 0, 1, 0 }, ColorSky, ColorGround);
	RETRO_DrawTerrainMesh(mesh, TerrainVertex, RETRO_ImageData(ASSET_TERRAIN), TEXTURE_SIZE, TEXTURE_SIZE, LightTable(ASSET_TERRAIN));
	RETRO_DrawTerrainModel(model, identity(), object, OBJECT_SCALE, Lighting ? Lights : Darkness, RETRO_ImageData(selected.texture), TEXTURE_SIZE, TEXTURE_SIZE, LightTable(selected.texture), &View);
	RETRO_DrawTerrainFlatModel(Models[MODEL_CUBE], identity(), Lights.light[LIGHT_GREEN].position, LIGHT_OBJECT_SCALE, Lighting ? ColorCubeGreen : ColorBlack, &View);
	RETRO_DrawTerrainFlatModel(Models[MODEL_CUBE], identity(), Lights.light[LIGHT_RED].position, LIGHT_OBJECT_SCALE, Lighting ? ColorCubeRed : ColorBlack, &View);

	// A shadow from each point light that is on, cast from where it is or
	// from straight above the object at its altitude
	for (int i = 0; i < POINT_LIGHTS; i++) {
		if (Lighting && Lights.light[i].on) {
			vec3 light = Lights.light[i].position;
			vec3 caster = CastFromAbove ? vec3{ object.x, light.y, object.z } : light;
			DrawShadow(model, object, OBJECT_SCALE, caster);
		}
	}

	DrawText();
}

void DEMO_Initialize(void)
{
	// The textures, each in a palette of its own
	RETRO_LoadImage("assets/shadow_terrain_256x256.pcx");
	RETRO_LoadImage("assets/shadow_earth_256x256.pcx");
	RETRO_LoadImage("assets/shadow_terrain2_256x256.pcx"); // the cube's sandstone
	RETRO_Image *heightmap = RETRO_LoadImage("assets/shadow_height_40x40.pcx");
	RETRO_SetTerrain(heightmap->width, heightmap->height, TERRAIN_SCALE, heightmap->data, NULL, false);

	Models[MODEL_SPHERE] = RETRO_Load3DModel("assets/shadow_sphere.obj");
	Models[MODEL_CUBE] = RETRO_Load3DModel("assets/shadow_cube.obj");

	FitScreenPalette();
	const RETRO_Palette *palette = ScreenPalette;

	// The colors drawn besides the textures, each held in the palette
	ColorBlack = RETRO_NearestPaletteIndex(RETRO_BLACK, palette);
	ColorTextGreen = RETRO_NearestPaletteIndex(RETRO_GREEN, palette);
	ColorWhite = RETRO_NearestPaletteIndex(RETRO_WHITE, palette);
	ColorCubeGreen = RETRO_NearestPaletteIndex(CubeGreen, palette);
	ColorCubeRed = RETRO_NearestPaletteIndex(CubeRed, palette);
	ColorSky = RETRO_NearestPaletteIndex(Sky, palette);
	ColorGround = RETRO_NearestPaletteIndex(Ground, palette);

	// A light table per texture, from its own colors to the screen's
	for (int texture = ASSET_TERRAIN; texture < ASSET_TERRAIN + TEXTURES; texture++) {
		RETRO_CreateShadeTable(RETRO_ImagePalette(texture), palette, LightTable(texture), RETRO_TintColor);
	}

	// Every entry, since a shadow can fall across a light's cube or past the
	// ground's edge onto the sky
	RETRO_CreateShadeTable(palette, RETRO_COLORS, 1, ShadowTable, SHADOW_LIGHT);

	// The lens: 90 degrees across, square pixels, pitched by the jeep
	RETRO_TerrainCamera.lens.focalx = RETRO_WIDTH / 2.0f;
	RETRO_TerrainCamera.lens.focaly = RETRO_WIDTH / 2.0f;
	RETRO_TerrainCamera.lens.center.y = RETRO_HEIGHT / 2.0f; // the vehicle pitches about the middle
	RETRO_TerrainView.step = 1;
	RETRO_TerrainView.distance = RETRO_Terrain.width * 3 / 2;
	RETRO_TerrainCamera.lens.nearplane = WORLD(10);

	// The camera starts 500 up and 400 short of the center, looking at it,
	// and falls onto the ground
	vec3 center = RETRO_TerrainCenter();
	RETRO_TerrainCamera.x = center.x;
	RETRO_TerrainCamera.z = center.z - WORLD(400);
	RETRO_TerrainCamera.height = WORLD(500);
	RETRO_TerrainCamera.heading = M_PI;

	// The hovering jeep the camera rides
	RETRO_Vehicle.acceleration = WORLD(900);
	RETRO_Vehicle.friction = WORLD(225);
	RETRO_Vehicle.maxspeed = WORLD(600);
	RETRO_Vehicle.turnspeed = radians(150);
	RETRO_Vehicle.gravity = WORLD(360);
	RETRO_Vehicle.spring = 22.5f;
	RETRO_Vehicle.lift = 10.7f; // -ln(0.7) * 30
	RETRO_Vehicle.clearance = WORLD(125);
	RETRO_Vehicle.floor = WORLD(50);
	RETRO_Vehicle.restpitch = RETRO_Vehicle.pitch = radians(10);
	RETRO_Vehicle.pitchkick = radians(0.6f) / WORLD(1);
	RETRO_Vehicle.pitchreturn = radians(10);

	RETRO_SetFont(RETRO_FONT_VGA_8X8);
}
