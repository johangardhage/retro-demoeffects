//
// Shadow 3
//
// A lit, textured landscape to drive over, an object orbiting above it that
// casts a dark shadow along the sun's rays onto the ground, and four lights -
// ambient, a sun high to one side, and a green and a white point light
// circling the world - with a cube marking each point light, and either can
// be lowered and raised. The shadow can instead be cast straight down, as if
// the sun were overhead.
//
// The world is measured in units of its own, 4000 across the 40 by 40 height
// map; WORLD() takes those to map cells.
//
// Only the green point light has a hue, so a vertex's light is two numbers:
// a neutral level, and how much further green reaches. Both are interpolated
// across a face and looked up, with the texel, in a table per texture that
// takes the texture's own palette onto the screen's:
//
//   table[t][n][g] = nearest(texture[t] · (n, min(n + g, 1), n))
//
// The shadow is the object's outline as the sun sees it: the convex outline
// of its corners seen along the sun's rays, each corner of it followed down
// its ray to where it meets the ground and lifted just above it. A cube casts
// a square and a sphere a round one; it stretches away from a low sun, and
// tilts and bends with a slope rather than floating over it. It is depth
// tested but writes no depth, and darkens what is under it through a table.
// With the sun out there is none.
//
// The screen's palette is fitted as the demo starts, to the colours those
// tables will ask for: each texture's colours, as often as its texels have
// them, under each light as often as the ground sees it with the point
// lights at points round their paths, and the ground's again in shadow.
// Changing a light changes the palette with it.
//
// Faces are cut at the near plane in the camera's frame before they are
// projected, so one reaching from behind the camera leaves no hole.
//
// Up/Down drive, Left/Right turn. L all lighting, A ambient, I sun, P both
// point lights, G and N the green and the white one, O next object, 1 and 2
// lower and raise the green light, 3 and 4 the white one, C cast from above
// or from the sun, H help.
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
#define POINT_LIGHT_KL 0.001f // green, attenuation per world unit
#define POINT_LIGHT2_KL 0.002f // white
#define POINT_LIGHT_RATE (90 * DEG2RAD) // radians a second round the world
#define POINT_LIGHT_RAISE WORLD(300) // a second, while 1 to 4 is held
#define POINT_LIGHT_TOP WORLD(2000) // the highest 1 to 4 raise a light, and the ground's base the lowest
#define LIGHT_LEVELS 32 // neutral light, in the tables
#define GREEN_LEVELS 16 // and green beyond it

#define OBJECT_SCALE WORLD(40)
#define OBJECT_ORBIT WORLD(200) // radius of the orbit about the world's centre
#define OBJECT_ALTITUDE WORLD(200) // mean height of the orbit
#define OBJECT_BOB WORLD(50) // and how far it rises and falls, three times a lap
#define OBJECT_RATE (90 * DEG2RAD) // radians a second round the orbit
#define LIGHT_OBJECT_SCALE WORLD(10)

#define SHADOW_LIFT WORLD(10) // above the ground it lies on
#define SHADOW_MAX_CORNERS 30 // on a shadow's outline; a model that needs more casts none
#define SHADOW_STEP WORLD(10) // along a ray, looking for the ground
#define SHADOW_LIGHT 0.5f // fraction of the light left in the shadow

#define PALETTE_TURNS 8 // points round the point lights' paths the palette is fitted at
#define OBJECT_WEIGHT (1 / 8.0f) // an object texture's say in the palette, beside the ground's
#define SHADOW_WEIGHT (1 / 4.0f) // and the ground's in shadow

#define TEXTURE_SIZE 256 // every texture, on a side

enum { ASSET_TERRAIN, ASSET_EARTH, ASSET_CUBE, ASSET_HEIGHTMAP };
#define TEXTURES (ASSET_HEIGHTMAP - ASSET_TERRAIN)
#define NO_TEXTURE -1
enum { MODEL_SPHERE, MODEL_CUBE, MODELS };
enum { LIGHT_GREEN, LIGHT_WHITE, POINT_LIGHTS };

struct Object {
	int model;
	int texture;
};

static const Object Objects[] = {
	{ MODEL_SPHERE, ASSET_EARTH },
	{ MODEL_CUBE, ASSET_CUBE }
};
static constexpr int OBJECTS = sizeof(Objects) / sizeof(Objects[0]);

// Light as summed, reduced to the two numbers that differ
struct Light {
	float neutral; // all three channels, capped at full
	float green;   // and how much further the green channel reaches
};

static unsigned char LightTableData[TEXTURES][RETRO_COLORS][LIGHT_LEVELS][GREEN_LEVELS];
static unsigned char ShadowTable[RETRO_COLORS];
static unsigned char ColorBlack, ColorTextGreen, ColorWhite, ColorCubeGreen, ColorSky;
static Model3D *Models[MODELS];
static const vec3 Sun = normalize(vec3{ 1, 1, -1 }); // the infinite light, towards it

// The screen's palette, and what it is fitted to
static RETRO_Palette ScreenPalette[RETRO_COLORS];
static RETRO_ColorHistogram Histogram;
static float LightWeight[LIGHT_LEVELS][GREEN_LEVELS]; // how often the ground sees each light

// The colours drawn besides the textures, held in the palette exactly. The
// sky is lavender, as it has always come out, where the book's is pink
static const RETRO_Palette CubeGreen = { 0, 255, 10 }, Sky = { 153, 153, 253 };
static const RETRO_Palette Held[] = { RETRO_BLACK, RETRO_GREEN, RETRO_WHITE, CubeGreen, Sky };
static constexpr int HELD = sizeof(Held) / sizeof(Held[0]);

static int CurrentObject;
static vec3 PointLightPosition[POINT_LIGHTS];
static float PointLightAltitude[POINT_LIGHTS] = { WORLD(200), WORLD(400) };

// The switches
static bool Lighting = true;
static bool AmbientLight = true;
static bool InfiniteLight = true;
static bool PointLight[POINT_LIGHTS] = { true, true };
static bool Help = true;
static bool CastFromAbove = false; // or from the sun

static vec3 WorldCenter(void)
{
	return { (RETRO_Terrain.width - 1) * 0.5f, 0, (RETRO_Terrain.height - 1) * 0.5f };
}

// The camera's frame, taken once a frame
static RETRO_TerrainBasis View;

static float PointLightTerm(vec3 p, vec3 n, vec3 light, float kl)
{
	vec3 l = light - p;
	float distance = length(l);
	if (distance <= 0) return 0;
	return POINT_LIGHT * MAX(dot(n, l) / distance, 0.0f) / (kl * TO_WORLD(distance));
}

// A colour under the light: the neutral level as the shade, and green
// beyond it as the tint
static RETRO_Palette Modulate(RETRO_Palette color, float neutral, const float *tint)
{
	float green = MIN(neutral + tint[0], 1.0f);
	return { (unsigned char)(color.r * neutral), (unsigned char)(color.g * green), (unsigned char)(color.b * neutral) };
}

// A vertex at p facing n, lit
static Light LightVertex(vec3 p, vec3 n)
{
	if (!Lighting) return { 0, 0 };

	float neutral = 0, green = 0;
	if (AmbientLight) neutral += AMBIENT_LIGHT;
	if (InfiniteLight) neutral += INFINITE_LIGHT * MAX(dot(n, Sun), 0.0f);
	if (PointLight[LIGHT_GREEN]) green += PointLightTerm(p, n, PointLightPosition[LIGHT_GREEN], POINT_LIGHT_KL);
	if (PointLight[LIGHT_WHITE]) neutral += PointLightTerm(p, n, PointLightPosition[LIGHT_WHITE], POINT_LIGHT2_KL);
	neutral = MIN(neutral, 1.0f);
	return { neutral, MIN(neutral + green, 1.0f) - neutral };
}

// A world point in the camera's frame, carrying its texture coordinates and
// its light as table levels: neutral as the shade, green as the tint
static RETRO_TerrainVertex MakeVertex(vec3 p, vec2 uv, Light light)
{
	RETRO_TerrainVertex vertex = {};
	vertex.eye = RETRO_TerrainPointEye(p.x, p.z, p.y, View);
	vertex.uv = uv;
	vertex.c = light.neutral * (LIGHT_LEVELS - 1) + 0.5f;
	vertex.tint[0] = light.green * (GREEN_LEVELS - 1) + 0.5f;
	return vertex;
}

// A texture's light table, from its own colours to the screen's
static RETRO_ShadeTable LightTable(int texture)
{
	return { &LightTableData[texture - ASSET_TERRAIN][0][0][0], RETRO_COLORS, LIGHT_LEVELS, { GREEN_LEVELS } };
}

//
// One polygon, through the near plane and onto the screen: textured and lit
// when there is a texture, a flat colour when there is not. Flat colours
// are black with the lighting off.
//
static void DrawPolygon(const RETRO_TerrainVertex *vertex, int count, int texture, unsigned char color)
{
	PolygonPoint polygon[RETRO_TERRAIN_MAX_POLYGON + 1];
	int points = RETRO_ClipProjectTerrainPolygon(vertex, count, polygon);
	if (points < 3) return;

	if (!Lighting) color = ColorBlack;
	if (texture != NO_TEXTURE) {
		RETRO_DrawTexMapGouraudPolygon(polygon, points, RETRO_ImageData(texture), TEXTURE_SIZE, TEXTURE_SIZE, LightTable(texture));
	} else {
		RETRO_DrawFlatPolygon(polygon, points, color);
	}
}

// The ground at sample x, z, lit
static RETRO_TerrainVertex TerrainVertex(int x, int z)
{
	vec3 p = { (float)x, RETRO_TerrainHeight(x, z), (float)z };
	vec3 n = normalize(RETRO_TerrainNormal(p.x, p.z));

	// The texture is stretched once over the patch
	vec2 uv = { p.x * (TEXTURE_SIZE - 1) / (RETRO_Terrain.width - 1), p.z * (TEXTURE_SIZE - 1) / (RETRO_Terrain.height - 1) };
	return MakeVertex(p, uv, LightVertex(p, n));
}

// Each visible cell as two triangles, split along the diagonal from its
// first corner to the opposite one
static void DrawTerrain(const RETRO_TerrainMesh &mesh)
{
	int step = mesh.step;
	for (int z = mesh.minz; z < mesh.maxz; z += step) {
		for (int x = mesh.minx; x < mesh.maxx; x += step) {
			if (!RETRO_TerrainCellVisible(mesh, x, z)) continue;
			RETRO_TerrainVertex first = TerrainVertex(x, z);
			RETRO_TerrainVertex across = TerrainVertex(x + step, z);
			RETRO_TerrainVertex down = TerrainVertex(x, z + step);
			RETRO_TerrainVertex opposite = TerrainVertex(x + step, z + step);
			RETRO_TerrainVertex upper[3] = { first, opposite, across };
			RETRO_TerrainVertex lower[3] = { first, down, opposite };
			DrawPolygon(upper, 3, ASSET_TERRAIN, 0);
			DrawPolygon(lower, 3, ASSET_TERRAIN, 0);
		}
	}
}

// One of the models at a position and scale
static void DrawModel(const Model3D *model, vec3 position, float scale, int texture, unsigned char color, bool lit)
{
	for (int i = 0; i < model->faces; i++) {
		const Face *face = &model->face[i];
		RETRO_TerrainVertex polygon[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			vec3 p = position + model->vertex[face->vertex[j]].pos * scale;
			Light light = lit ? LightVertex(p, model->normal[face->vertexnormal[j]].dir) : Light{ 1, 0 };
			polygon[j] = MakeVertex(p, model->uv[face->uv[j]], light);
		}
		DrawPolygon(polygon, face->vertices, texture, color);
	}
}

// The shadow of a model at a position and scale, cast along parallel rays
// from the direction sun
static void DrawShadow(const Model3D *model, vec3 position, float scale, vec3 sun)
{
	if (!Lighting || !InfiniteLight) return;

	vec3 center;
	if (!RETRO_TerrainRayHit(position, -sun, SHADOW_STEP, center)) return;
	int x = (int)floorf(center.x);
	int z = (int)floorf(center.z);
	if (x < 0 || z < 0 || x >= RETRO_Terrain.width - 1 || z >= RETRO_Terrain.height - 1) return;

	// Two directions across the rays, taken off whichever axis is not along
	// them, and the model's corners seen along the rays
	vec3 axis = fabsf(sun.y) < 0.99f ? vec3{ 0, 1, 0 } : vec3{ 0, 0, 1 };
	vec3 across = normalize(cross(sun, axis));
	vec3 up = cross(across, sun);
	vec2 seen[RETRO_MAX_VERTICES];
	for (int i = 0; i < model->vertices; i++) {
		seen[i] = { dot(model->vertex[i].pos, across), dot(model->vertex[i].pos, up) };
	}
	int outline[SHADOW_MAX_CORNERS];
	int corners = RETRO_ConvexOutline(seen, model->vertices, outline, SHADOW_MAX_CORNERS);
	if (corners < 3) return;

	RETRO_TerrainVertex shadow[SHADOW_MAX_CORNERS];
	for (int i = 0; i < corners; i++) {
		vec3 corner = position + model->vertex[outline[i]].pos * scale;
		vec3 p;
		if (!RETRO_TerrainRayHit(corner, -sun, SHADOW_STEP, p)) return;
		shadow[i] = MakeVertex({ p.x, p.y + SHADOW_LIFT, p.z }, { 0, 0 }, { 1, 0 });
	}
	// Each corner lands at its own height, so the outline is not flat and can
	// turn concave on screen, where a fan from one of its corners reaches
	// past it. Fanned from the centre's shadow instead, the triangles stay
	// inside the outline; each is clipped on its own, so the near plane
	// cannot move the fan off that centre
	RETRO_TerrainVertex middle = MakeVertex({ center.x, center.y + SHADOW_LIFT, center.z }, { 0, 0 }, { 1, 0 });
	for (int i = 0; i < corners; i++) {
		RETRO_TerrainVertex triangle[3] = { middle, shadow[i], shadow[(i + 1) % corners] };
		PolygonPoint polygon[4];
		int points = RETRO_ClipProjectTerrainPolygon(triangle, 3, polygon);
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
						"<G>/<N> Toggle green/white light\n"
						"<O> Select different objects\n"
						"<1>/<2> Lower/raise green light\n"
						"<3>/<4> Lower/raise white light\n"
						"<C> Cast from above/sun\n"
						"<H> Toggle Help\n"
						"<Q> Exit demo", 0, 14, ColorWhite);
	}

	// CELL is the cell the camera is over
	vec3 center = WorldCenter();
	// Each line within the screen's 40 columns
	snprintf(text, sizeof(text), "CAM [%5.0f,%5.0f,%5.0f] CELL [%d, %d]\nLighting [%s]: Amb=%d Inf=%d G=%d W=%d\nGreen y=%.0f White y=%.0f\nShadow cast from [%s]",
			 TO_WORLD(RETRO_Camera.x - center.x), TO_WORLD(RETRO_Camera.height), TO_WORLD(RETRO_Camera.z - center.z), (int)floorf(RETRO_Camera.x), (int)floorf(RETRO_Camera.z),
			 Lighting ? "ON" : "OFF", AmbientLight, InfiniteLight, PointLight[LIGHT_GREEN], PointLight[LIGHT_WHITE], TO_WORLD(PointLightAltitude[LIGHT_GREEN]), TO_WORLD(PointLightAltitude[LIGHT_WHITE]),
			 CastFromAbove ? "ABOVE" : "SUN");
	RETRO_PutString(text, 0, RETRO_HEIGHT - 37, ColorTextGreen);
}

// The point lights on their paths about the world's centre
static void PlacePointLights(double time)
{
	vec3 center = WorldCenter();
	float angle = fmod(time * POINT_LIGHT_RATE, 2 * M_PI);
	PointLightPosition[LIGHT_GREEN] = { center.x - WORLD(1000) * cosf(angle), PointLightAltitude[LIGHT_GREEN], center.z + WORLD(1000) * sinf(angle) };
	PointLightPosition[LIGHT_WHITE] = { center.x - WORLD(500) * cosf(-2 * angle), PointLightAltitude[LIGHT_WHITE], center.z + WORLD(1000) * sinf(-2 * angle) };
}

//
// The screen's palette, fitted to the colours the light tables will ask for
//
static void FitScreenPalette(void)
{
	// How often the ground sees each light the tables hold, rounded to its
	// levels as MakeVertex rounds it, with the point lights at points round
	// their paths
	for (int turn = 0; turn < PALETTE_TURNS; turn++) {
		PlacePointLights(turn * 2 * M_PI / PALETTE_TURNS / POINT_LIGHT_RATE);
		for (int z = 0; z < RETRO_Terrain.height; z++) {
			for (int x = 0; x < RETRO_Terrain.width; x++) {
				vec3 p = { (float)x, RETRO_TerrainHeight(x, z), (float)z };
				Light light = LightVertex(p, normalize(RETRO_TerrainNormal(p.x, p.z)));
				int c = light.neutral * (LIGHT_LEVELS - 1) + 0.5f;
				int green = light.green * (GREEN_LEVELS - 1) + 0.5f;
				LightWeight[c][green]++;
			}
		}
	}

	// Each texture's colours, as often as its texels have them, under those
	// lights: the ground's, then the objects' with less say, since they
	// cover less of the screen, and the ground's again in shadow
	for (int texture = ASSET_TERRAIN; texture < ASSET_TERRAIN + TEXTURES; texture++) {
		if (texture == ASSET_TERRAIN) {
			RETRO_AddShadeTableColors(&Histogram, texture, LightTable(texture), &LightWeight[0][0], Modulate);
			RETRO_AddShadeTableColors(&Histogram, texture, LightTable(texture), &LightWeight[0][0], Modulate, SHADOW_WEIGHT, SHADOW_LIGHT);
		} else {
			RETRO_AddShadeTableColors(&Histogram, texture, LightTable(texture), &LightWeight[0][0], Modulate, OBJECT_WEIGHT);
		}
	}

	RETRO_CreateHistogramPalette(&Histogram, ScreenPalette, Held, HELD);
	RETRO_SetPalette(ScreenPalette);
}

void DEMO_FixedUpdate(double timestep)
{
	RETRO_UpdateTerrainVehicle(timestep);
}

void DEMO_Render(double time, double deltatime)
{
	if (RETRO_KeyPressed(SDL_SCANCODE_L)) Lighting = !Lighting;
	if (RETRO_KeyPressed(SDL_SCANCODE_A)) AmbientLight = !AmbientLight;
	if (RETRO_KeyPressed(SDL_SCANCODE_I)) InfiniteLight = !InfiniteLight;
	if (RETRO_KeyPressed(SDL_SCANCODE_P)) {
		// Both on unless both already are
		bool on = !(PointLight[LIGHT_GREEN] && PointLight[LIGHT_WHITE]);
		PointLight[LIGHT_GREEN] = PointLight[LIGHT_WHITE] = on;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_G)) PointLight[LIGHT_GREEN] = !PointLight[LIGHT_GREEN];
	if (RETRO_KeyPressed(SDL_SCANCODE_N)) PointLight[LIGHT_WHITE] = !PointLight[LIGHT_WHITE];
	if (RETRO_KeyPressed(SDL_SCANCODE_H)) Help = !Help;
	if (RETRO_KeyPressed(SDL_SCANCODE_O)) CurrentObject = (CurrentObject + 1) % OBJECTS;
	if (RETRO_KeyPressed(SDL_SCANCODE_C)) CastFromAbove = !CastFromAbove;
	float raise = POINT_LIGHT_RAISE * deltatime;
	if (RETRO_KeyState(SDL_SCANCODE_1)) PointLightAltitude[LIGHT_GREEN] -= raise;
	if (RETRO_KeyState(SDL_SCANCODE_2)) PointLightAltitude[LIGHT_GREEN] += raise;
	if (RETRO_KeyState(SDL_SCANCODE_3)) PointLightAltitude[LIGHT_WHITE] -= raise;
	if (RETRO_KeyState(SDL_SCANCODE_4)) PointLightAltitude[LIGHT_WHITE] += raise;
	for (float &altitude : PointLightAltitude) {
		altitude = MIN(MAX(altitude, 0.0f), POINT_LIGHT_TOP);
	}

	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	View = mesh.basis;

	PlacePointLights(time);
	vec3 center = WorldCenter();

	// The object on its orbit, bobbing three times a lap
	float orbit = fmod(time * OBJECT_RATE, 2 * M_PI);
	vec3 object = { center.x - OBJECT_ORBIT * cosf(orbit), OBJECT_ALTITUDE + OBJECT_BOB * sinf(3 * orbit), center.z + OBJECT_ORBIT * sinf(orbit) };
	const Object &selected = Objects[CurrentObject];

	RETRO_Clear(ColorSky);
	RETRO_ClearDepthBuffer();
	DrawTerrain(mesh);
	DrawModel(Models[selected.model], object, OBJECT_SCALE, selected.texture, 0, true);
	DrawModel(Models[MODEL_CUBE], PointLightPosition[LIGHT_GREEN], LIGHT_OBJECT_SCALE, NO_TEXTURE, ColorCubeGreen, false);
	DrawModel(Models[MODEL_CUBE], PointLightPosition[LIGHT_WHITE], LIGHT_OBJECT_SCALE, NO_TEXTURE, ColorWhite, false);
	// The direction the shadow is cast from
	vec3 caster = CastFromAbove ? vec3{ 0, 1, 0 } : Sun;
	DrawShadow(Models[selected.model], object, OBJECT_SCALE, caster);
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

	// The colours drawn besides the textures, each held in the palette
	ColorBlack = RETRO_NearestPaletteIndex(RETRO_BLACK, palette);
	ColorTextGreen = RETRO_NearestPaletteIndex(RETRO_GREEN, palette);
	ColorWhite = RETRO_NearestPaletteIndex(RETRO_WHITE, palette);
	ColorCubeGreen = RETRO_NearestPaletteIndex(CubeGreen, palette);
	ColorSky = RETRO_NearestPaletteIndex(Sky, palette);

	// A light table per texture, from its own colours to the screen's
	for (int texture = ASSET_TERRAIN; texture < ASSET_TERRAIN + TEXTURES; texture++) {
		RETRO_CreateShadeTable(RETRO_ImagePalette(texture), palette, LightTable(texture), Modulate);
	}

	// Every entry, since the shadow can fall across a light's cube or past the
	// ground's edge onto the sky
	RETRO_CreateShadeTable(palette, RETRO_COLORS, 1, ShadowTable, SHADOW_LIGHT);

	// The lens: 90 degrees across, square pixels, pitched by the jeep
	RETRO_TerrainView.focalx = RETRO_WIDTH / 2.0f;
	RETRO_TerrainView.focaly = RETRO_WIDTH / 2.0f;
	RETRO_TerrainView.step = 1;
	RETRO_TerrainView.distance = RETRO_Terrain.width * 3 / 2;
	RETRO_TerrainView.nearplane = WORLD(10);

	// The camera starts 500 up and 300 short of the centre, looking at it,
	// and falls onto the ground
	vec3 center = WorldCenter();
	RETRO_Camera.x = center.x;
	RETRO_Camera.z = center.z - WORLD(300);
	RETRO_Camera.height = WORLD(500);
	RETRO_Camera.heading = M_PI;

	// The hovering jeep the camera rides
	RETRO_Vehicle.acceleration = WORLD(900);
	RETRO_Vehicle.friction = WORLD(225);
	RETRO_Vehicle.maxspeed = WORLD(600);
	RETRO_Vehicle.turnspeed = 150 * DEG2RAD;
	RETRO_Vehicle.gravity = WORLD(360);
	RETRO_Vehicle.spring = 22.5f;
	RETRO_Vehicle.lift = 10.7f; // -ln(0.7) * 30
	RETRO_Vehicle.clearance = WORLD(125);
	RETRO_Vehicle.floor = WORLD(50);
	RETRO_Vehicle.restpitch = RETRO_Vehicle.pitch = 10 * DEG2RAD;
	RETRO_Vehicle.pitchkick = 0.6f * DEG2RAD / WORLD(1);
	RETRO_Vehicle.pitchreturn = 10 * DEG2RAD;

	RETRO_SetFont(RETRO_FONT_VGA_8X8);
}
