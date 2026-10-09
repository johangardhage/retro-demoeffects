//
// Shadow 3
//
// A flat, lit, sandstone plain to drive over, a fan spinning over its center,
// and four lights - ambient, a sun, and a green and a red point light
// circling the world - with a cube marking each point light. Each point light
// throws a shadow of the fan onto a plane just over the ground, and either
// can be lowered and raised: the nearer it comes down to the fan, the larger
// its shadow. Either shadow can instead be cast from straight above the
// fan, at its light's altitude.
//
// The world is measured in units of its own, 1000 across the 16 by 16 height
// map; WORLD() takes those to map cells. The map is all zeros, so it is
// made here rather than loaded.
//
// Two point lights have a hue, so a vertex's light is three numbers: a
// neutral level, and how much further red and green reach. All three are
// interpolated across a face and looked up, with the texel, in a table per
// texture that takes the texture's own palette onto the screen's:
//
//   table[t][n][r][g] = nearest(texture[t] · (min(n + r, 1), min(n + g, 1), n))
//
// The fan is untextured, a plain gray, and is lit like the textures through
// a table of its own, as a texture of one texel.
//
// Each shadow is the object itself, flattened: every corner of it is
// followed along its ray from the light to the level plane just above the
// ground, where a corner at height h under a light at height l lands
// (l - plane) / (l - h) as far from the light as it started. Only the faces
// turned toward the light are drawn there, in one pass that darkens each
// pixel once however many of them cover it, so the fan's hub and blades,
// overlapping, still cast one even shadow. It is depth tested but writes no
// depth, and darkens what is under it through a table, so where the two
// lights' shadows cross it is darker still. With a point light out, or down
// level with any of the object, there is none from it.
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
#include "lib/retroterrain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"
#include "lib/retrofont.h"
#include "lib/retrovector.h"

// The terrain is 1000 world units across 16 samples
#define WORLD(units) ((units) * (16 - 1) / 1000.0f)
#define TO_WORLD(cells) ((cells) * 1000.0f / (16 - 1))

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

#define OBJECT_SCALE WORLD(10)
#define OBJECT_ALTITUDE WORLD(150) // over the world's center
#define OBJECT_RATE radians(60) // radians a second it spins
#define LIGHT_OBJECT_SCALE WORLD(10)

#define SHADOW_PLANE WORLD(10) // height of the plane the shadows lie on
#define SHADOW_LIGHT (5 / 7.0f) // fraction of the light left in the shadow

#define PALETTE_TURNS 8 // points round the point lights' paths the palette is fitted at
#define OBJECT_WEIGHT (1 / 8.0f) // an object texture's say in the palette, beside the ground's
#define SHADOW_WEIGHT (1 / 4.0f) // and the ground's in shadow

#define TEXTURE_SIZE 256 // every texture, on a side

enum { ASSET_TERRAIN, ASSET_EARTH, ASSETS };
#define TEXTURES (ASSETS - ASSET_TERRAIN)
#define GRAY -2 // no texture, but lit: the fan's plain gray
enum { MODEL_FAN, MODEL_SPHERE, MODEL_CUBE, MODELS };
enum { LIGHT_RED, LIGHT_GREEN, POINT_LIGHTS, LIGHT_SUN = POINT_LIGHTS, LIGHTS };

struct Object {
	int model;
	int texture;
};

static const Object Objects[] = {
	{ MODEL_FAN, GRAY },
	{ MODEL_SPHERE, ASSET_EARTH },
	{ MODEL_CUBE, ASSET_TERRAIN }
};
#define OBJECTS (int)(sizeof(Objects) / sizeof(Objects[0]))

static unsigned char LightTableData[TEXTURES][RETRO_COLORS][LIGHT_LEVELS][TINT_LEVELS][TINT_LEVELS];
static unsigned char GrayTable[LIGHT_LEVELS][TINT_LEVELS][TINT_LEVELS]; // and on the fan's gray
static unsigned char GrayTexel; // the one texel of the gray, the table's only row
static unsigned char ShadowTable[RETRO_COLORS];
static unsigned char ColorBlack, ColorTextGreen, ColorWhite, ColorCubeGreen, ColorCubeRed, ColorSky, ColorGround;
static Model3D *Models[MODELS];
static unsigned char FlatGround[16 * 16]; // the height map
static const RETRO_Palette Gray = { 191, 191, 191 };

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

// The fan's gray's, as a texture of one texel
static RETRO_ShadeTable GrayLightTable(void)
{
	return { &GrayTable[0][0][0], 1, LIGHT_LEVELS, { TINT_LEVELS, TINT_LEVELS } };
}

// The ground's corner at p, as the mesh's camera sees it, lit
static RETRO_CameraVertex TerrainVertex(vec3 p, const RETRO_CameraVertex &corner)
{
	vec3 n = normalize(RETRO_TerrainNormal(p.x, p.z));

	// The texture is stretched once over the patch
	vec2 uv = { p.x * (TEXTURE_SIZE - 1) / (RETRO_Terrain.width - 1), p.z * (TEXTURE_SIZE - 1) / (RETRO_Terrain.height - 1) };
	return RETRO_TerrainTintVertex(corner, uv, LightVertex(p, n), LightTable(ASSET_TERRAIN));
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
			 TO_WORLD(RETRO_Rider.x - center.x), TO_WORLD(RETRO_Rider.height), TO_WORLD(RETRO_Rider.z - center.z), (int)floorf(RETRO_Rider.x), (int)floorf(RETRO_Rider.z),
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
	float texels = TEXTURE_SIZE * TEXTURE_SIZE; // as many as a texture's
	RETRO_AddShadeTableColors(&Histogram, &Gray, &texels, GrayLightTable(), LightWeight, RETRO_TintColor, OBJECT_WEIGHT);

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

	// The object over the center, spinning. The world is the mirror of the
	// book's, so it turns the other way round y to look the same
	float ay = fmod(time.total * OBJECT_RATE, 2 * M_PI);
	mat3 spin = rotateY(-ay);
	vec3 object = { center.x, OBJECT_ALTITUDE, center.z };
	const Object &selected = Objects[currentobject];
	const Model3D *model = Models[selected.model];

	// The sky, and the ground color below the camera's horizon for wherever
	// the landscape does not reach, so it follows the vehicle's pitch
	RETRO_ClearDepthBuffer();
	const ClipRect &view = View.lens.view;
	RETRO_DrawRectangle(view.x0, view.y0, view.x1 - 1, view.y1 - 1, ColorSky);
	PolygonPoint below[5];
	int count = RETRO_ClipHorizon(&View, { 0, 1, 0 }, below);
	if (count >= 3) RETRO_DrawFlatPolygon(below, count, ColorGround, view);
	RETRO_DrawTerrainMesh(mesh, TerrainVertex, RETRO_ImageData(ASSET_TERRAIN), TEXTURE_SIZE, TEXTURE_SIZE, LightTable(ASSET_TERRAIN));
	if (selected.texture == GRAY) {
		RETRO_DrawTerrainModel(model, spin, object, OBJECT_SCALE, Lighting ? Lights : Darkness, &GrayTexel, 1, 1, GrayLightTable(), &View);
	} else {
		RETRO_DrawTerrainModel(model, spin, object, OBJECT_SCALE, Lighting ? Lights : Darkness, RETRO_ImageData(selected.texture), TEXTURE_SIZE, TEXTURE_SIZE, LightTable(selected.texture), &View);
	}
	RETRO_DrawTerrainFlatModel(Models[MODEL_CUBE], identity(), Lights.light[LIGHT_GREEN].position, LIGHT_OBJECT_SCALE, Lighting ? ColorCubeGreen : ColorBlack, &View);
	RETRO_DrawTerrainFlatModel(Models[MODEL_CUBE], identity(), Lights.light[LIGHT_RED].position, LIGHT_OBJECT_SCALE, Lighting ? ColorCubeRed : ColorBlack, &View);

	// A shadow from each point light that is on, cast from where it is or
	// from straight above the object at its altitude
	for (int i = 0; i < POINT_LIGHTS; i++) {
		if (Lighting && Lights.light[i].on) {
			vec3 light = Lights.light[i].position;
			vec3 caster = CastFromAbove ? vec3{ object.x, light.y, object.z } : light;
			RETRO_DrawTerrainShadow(model, spin, object, OBJECT_SCALE, caster, SHADOW_PLANE, ShadowTable, &View);
		}
	}

	DrawText();
}

void DEMO_Initialize(void)
{
	// The textures, each in a palette of its own
	RETRO_LoadImage("assets/shadow_terrain2_256x256.pcx");
	RETRO_LoadImage("assets/shadow_earth_256x256.pcx");
	RETRO_SetTerrain(16, 16, TERRAIN_SCALE, FlatGround, NULL, false);

	Models[MODEL_FAN] = RETRO_Load3DModel("assets/shadow_fan.obj");
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

	// A light table per texture, from its own colors to the screen's, and
	// one of the fan's gray
	for (int texture = ASSET_TERRAIN; texture < ASSET_TERRAIN + TEXTURES; texture++) {
		RETRO_CreateShadeTable(RETRO_ImagePalette(texture), palette, LightTable(texture), RETRO_TintColor);
	}
	RETRO_CreateShadeTable(&Gray, palette, GrayLightTable(), RETRO_TintColor);

	// Every entry, since the shadow plane reaches past the ground's edge,
	// over the ground band and the sky
	RETRO_CreateShadeTable(palette, RETRO_COLORS, 1, ShadowTable, SHADOW_LIGHT);

	// The lens: 90 degrees across, square pixels, pitched by the jeep
	RETRO_Rider.lens.focalx = RETRO_WIDTH / 2.0f;
	RETRO_Rider.lens.focaly = RETRO_WIDTH / 2.0f;
	RETRO_Rider.lens.center.y = RETRO_HEIGHT / 2.0f; // the vehicle pitches about the middle
	RETRO_TerrainDraw.step = 1;
	RETRO_TerrainDraw.distance = RETRO_Terrain.width * 3 / 2;
	RETRO_Rider.lens.nearplane = WORLD(10);

	// The camera starts 500 up and 400 short of the center, looking at it,
	// and falls onto the ground
	vec3 center = RETRO_TerrainCenter();
	RETRO_Rider.x = center.x;
	RETRO_Rider.z = center.z - WORLD(400);
	RETRO_Rider.height = WORLD(500);
	RETRO_Rider.heading = M_PI;

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
