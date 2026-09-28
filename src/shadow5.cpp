//
// Shadow 5
//
// A flat, lit, sandstone plain to drive over, a fan spinning over its centre,
// and four lights - ambient, a sun, and a green and a red point light
// circling the world - with a cube marking each point light, and either can
// be lowered and raised. The fan's shadow is not cast by any of them, but
// painted: lightmaps darken the sandstone under it.
//
// The world is measured in units of its own, 1000 across the 16 by 16 height
// map; WORLD() takes those to map cells. The map is all zeros, so it is
// made here rather than loaded.
//
// Two point lights have a hue, so a vertex's light is three numbers: a
// neutral level, and how much further green and red reach. All three are
// interpolated across a face and looked up, with the texel, in a table per
// texture that takes the texture's own palette onto the screen's:
//
//   table[t][n][g][r] = nearest(texture[t] · (min(n + r, 1), min(n + g, 1), n))
//
// The fan is untextured, a plain grey, and is lit like the textures through
// a table of its own, as a texture of one texel.
//
// A lightmap is a level of light for every texel of the ground's texture,
// stretched over the ground as the texture is. Each texel is darkened to its
// level before the light above is added, through a table from the texture's
// palette onto itself:
//
//   texture[x, y] = nearest(sandstone[x, y] · lightmap[x, y])
//
// The fan's four blades repeat every quarter turn, so nine lightmaps, a
// ninth of that apart, hold every turn of its shadow, and the one nearest the
// fan's own turn is laid on the texture whenever it changes. The texture's
// palette holds the sandstone's shadowed colours besides its own.
//
// The screen's palette is fitted as the demo starts, to the colours those
// tables will ask for: the ground's, as often as its texels have them under
// the lightmaps, and the fan's grey, each under each light as often as the
// ground sees it with the point lights at points round their paths.
// Changing a light changes the palette with it.
//
// Faces are cut at the near plane in the camera's frame before they are
// projected, so one reaching from behind the camera leaves no hole.
//
// Up/Down drive, Left/Right turn. L all lighting, A ambient, I sun, P both
// point lights, G and R the green and the red one, 1 and 2 lower and raise
// the green light, 3 and 4 the red one, H help.
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

// The terrain is 1000 world units across 16 samples
#define WORLD(units) ((units) * (16 - 1) / 1000.0f)
#define TO_WORLD(cells) ((cells) * 1000.0f / (16 - 1))

#define TERRAIN_SCALE WORLD(700.0f / 255) // map cells of height per stored byte

#define AMBIENT_LIGHT (100 / 256.0f)
#define INFINITE_LIGHT (100 / 256.0f)
#define POINT_LIGHT (255 / 256.0f)
#define POINT_LIGHT_KL 0.001f // green, attenuation per world unit
#define POINT_LIGHT2_KL 0.002f // red
#define POINT_LIGHT_RATE (30 * DEG2RAD) // radians a second round the world
#define POINT_LIGHT_ORBIT WORLD(500) // green, radius about the world's centre
#define POINT_LIGHT2_ORBIT WORLD(200) // red, twice as fast the other way
#define POINT_LIGHT_ALTITUDE WORLD(500) // where both start
#define POINT_LIGHT_RAISE WORLD(300) // a second, while 1 to 4 is held
#define POINT_LIGHT_TOP WORLD(2000) // the highest 1 to 4 raise a light, and the ground's base the lowest
#define LIGHT_LEVELS 32 // neutral light, in the tables
#define TINT_LEVELS 8 // and each point light's colour beyond it

#define OBJECT_SCALE WORLD(20)
#define OBJECT_ALTITUDE WORLD(200) // over the world's centre
#define OBJECT_RATE (300 * DEG2RAD) // radians a second it spins
#define LIGHT_OBJECT_SCALE WORLD(10)

#define TEXTURE_SIZE 256 // every texture, on a side

#define LIGHTMAPS 9 // the fan's shadow, one after the other in their image
#define LIGHTMAP_TURN (10 * DEG2RAD) // radians the fan turns from one to the next
#define NO_LIGHTMAP -1

#define PALETTE_TURNS 8 // points round the point lights' paths the palette is fitted at
#define OBJECT_WEIGHT (1 / 8.0f) // an object texture's say in the palette, beside the ground's

enum { ASSET_TERRAIN, ASSET_LIGHTMAPS };
#define NO_TEXTURE -1
#define GREY -2 // no texture, but lit: the fan's plain grey
enum { MODEL_FAN, MODEL_CUBE, MODELS };
enum { LIGHT_GREEN, LIGHT_RED, POINT_LIGHTS };

// Light as summed, reduced to the numbers that differ
struct Light {
	float neutral;				// all three channels, capped at full
	float tint[POINT_LIGHTS];	// and how much further each point light's channel reaches
};
static_assert(POINT_LIGHTS <= RETRO_MAX_TINTS, "Each point light needs a tint of its own");

static unsigned char LightTableData[RETRO_COLORS][LIGHT_LEVELS][TINT_LEVELS][TINT_LEVELS]; // the ground's
static unsigned char GreyTable[LIGHT_LEVELS][TINT_LEVELS][TINT_LEVELS]; // and on the fan's grey
static unsigned char GreyTexel; // the one texel of the grey, the table's only row
static unsigned char LightmapTable[RETRO_COLORS][RETRO_COLORS]; // a texel at each lightmap level
static unsigned char Sandstone[TEXTURE_SIZE * TEXTURE_SIZE]; // the ground's texture under no lightmap
static int AppliedLightmap = NO_LIGHTMAP; // none yet
static unsigned char ColorBlack, ColorTextGreen, ColorWhite, ColorCubeGreen, ColorCubeRed, ColorSky, ColorGround;
static Model3D *Models[MODELS];
static unsigned char FlatGround[16 * 16]; // the height map
static const RETRO_Palette Grey = { 191, 191, 191 };
static const vec3 Sun = normalize(vec3{ 1, 1, -1 }); // the infinite light, towards it

// The screen's palette, and what it is fitted to
static RETRO_Palette ScreenPalette[RETRO_COLORS];
static RETRO_ColorHistogram Histogram;
static float LightWeight[LIGHT_LEVELS][TINT_LEVELS][TINT_LEVELS]; // how often the ground sees each light

// The colours drawn besides the textures, held in the palette exactly
static const RETRO_Palette CubeGreen = { 0, 255, 10 }, CubeRed = { 255, 0, 0 }, Sky = { 50, 50, 200 }, Ground = { 25, 50, 110 };
static const RETRO_Palette Held[] = { RETRO_BLACK, RETRO_GREEN, RETRO_WHITE, CubeGreen, CubeRed, Sky, Ground };
static constexpr int HELD = sizeof(Held) / sizeof(Held[0]);

static const float PointLightKL[POINT_LIGHTS] = { POINT_LIGHT_KL, POINT_LIGHT2_KL };
static vec3 PointLightPosition[POINT_LIGHTS];
static float PointLightAltitude[POINT_LIGHTS] = { POINT_LIGHT_ALTITUDE, POINT_LIGHT_ALTITUDE };

// The switches
static bool Lighting = true;
static bool AmbientLight = true;
static bool InfiniteLight = true;
static bool PointLight[POINT_LIGHTS] = { true, true };
static bool Help = true;

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

// A colour under the light: the neutral level as the shade, and green and
// red beyond it as the tints
static RETRO_Palette Modulate(RETRO_Palette color, float neutral, const float *tint)
{
	float green = MIN(neutral + tint[LIGHT_GREEN], 1.0f);
	float red = MIN(neutral + tint[LIGHT_RED], 1.0f);
	return { (unsigned char)(color.r * red), (unsigned char)(color.g * green), (unsigned char)(color.b * neutral) };
}

// A vertex at p facing n, lit
static Light LightVertex(vec3 p, vec3 n)
{
	Light light = {};
	if (!Lighting) return light;

	if (AmbientLight) light.neutral += AMBIENT_LIGHT;
	if (InfiniteLight) light.neutral += INFINITE_LIGHT * MAX(dot(n, Sun), 0.0f);
	light.neutral = MIN(light.neutral, 1.0f);
	for (int i = 0; i < POINT_LIGHTS; i++) {
		if (PointLight[i]) {
			float term = PointLightTerm(p, n, PointLightPosition[i], PointLightKL[i]);
			light.tint[i] = MIN(light.neutral + term, 1.0f) - light.neutral;
		}
	}
	return light;
}

// A world point in the camera's frame, carrying its texture coordinates and
// its light as table levels: neutral as the shade, green and red as the tints
static RETRO_TerrainVertex MakeVertex(vec3 p, vec2 uv, Light light)
{
	RETRO_TerrainVertex vertex = {};
	vertex.eye = RETRO_TerrainPointEye(p.x, p.z, p.y, View);
	vertex.uv = uv;
	vertex.c = light.neutral * (LIGHT_LEVELS - 1) + 0.5f;
	for (int i = 0; i < POINT_LIGHTS; i++) {
		vertex.tint[i] = light.tint[i] * (TINT_LEVELS - 1) + 0.5f;
	}
	return vertex;
}

// The ground's light table, from its texture's own colours to the screen's
static RETRO_ShadeTable LightTable(void)
{
	return { &LightTableData[0][0][0][0], RETRO_COLORS, LIGHT_LEVELS, { TINT_LEVELS, TINT_LEVELS } };
}

// The fan's grey's, as a texture of one texel
static RETRO_ShadeTable GreyLightTable(void)
{
	return { &GreyTable[0][0][0], 1, LIGHT_LEVELS, { TINT_LEVELS, TINT_LEVELS } };
}

//
// One polygon, through the near plane and onto the screen: the ground's
// texture, lit, for ASSET_TERRAIN, grey and lit for GREY, a flat colour
// otherwise.
// Flat colours are black with the lighting off.
//
static void DrawPolygon(const RETRO_TerrainVertex *vertex, int count, int texture, unsigned char color)
{
	PolygonPoint polygon[RETRO_TERRAIN_MAX_POLYGON + 1];
	int points = RETRO_ClipProjectTerrainPolygon(vertex, count, polygon);
	if (points < 3) return;

	if (!Lighting) color = ColorBlack;
	if (texture == GREY) {
		RETRO_DrawTexMapGouraudPolygon(polygon, points, &GreyTexel, 1, 1, GreyLightTable());
	} else if (texture != NO_TEXTURE) {
		RETRO_DrawTexMapGouraudPolygon(polygon, points, RETRO_ImageData(texture), TEXTURE_SIZE, TEXTURE_SIZE, LightTable());
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

// One of the models turned, at a position and scale
static void DrawModel(const Model3D *model, const mat3 &rotation, vec3 position, float scale, int texture, unsigned char color, bool lit)
{
	for (int i = 0; i < model->faces; i++) {
		const Face *face = &model->face[i];
		RETRO_TerrainVertex polygon[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			vec3 p = position + rotation * model->vertex[face->vertex[j]].pos * scale;
			Light light = lit ? LightVertex(p, rotation * model->normal[face->vertexnormal[j]].dir) : Light{ 1, {} };
			polygon[j] = MakeVertex(p, model->uv[face->uv[j]], light);
		}
		DrawPolygon(polygon, face->vertices, texture, color);
	}
}

//
// The ground's texture under a lightmap
//
static void ApplyLightmap(int lightmap)
{
	unsigned char *texture = RETRO_ImageData(ASSET_TERRAIN);
	const unsigned char *level = RETRO_ImageData(ASSET_LIGHTMAPS) + lightmap * TEXTURE_SIZE * TEXTURE_SIZE;
	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) {
		texture[i] = LightmapTable[Sandstone[i]][level[i]];
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
						"<1>/<2> Lower/raise green light\n"
						"<3>/<4> Lower/raise red light\n"
						"<H> Toggle Help\n"
						"<Q> Exit demo", 0, 14, ColorWhite);
	}

	// CELL is the cell the camera is over
	vec3 center = WorldCenter();
	// Each line within the screen's 40 columns
	snprintf(text, sizeof(text), "CAM [%5.0f,%5.0f,%5.0f] CELL [%d, %d]\nLighting [%s]: Amb=%d Inf=%d G=%d R=%d\nGreen y=%.0f Red y=%.0f",
			 TO_WORLD(RETRO_Camera.x - center.x), TO_WORLD(RETRO_Camera.height), TO_WORLD(RETRO_Camera.z - center.z), (int)floorf(RETRO_Camera.x), (int)floorf(RETRO_Camera.z),
			 Lighting ? "ON" : "OFF", AmbientLight, InfiniteLight, PointLight[LIGHT_GREEN], PointLight[LIGHT_RED], TO_WORLD(PointLightAltitude[LIGHT_GREEN]), TO_WORLD(PointLightAltitude[LIGHT_RED]));
	RETRO_PutString(text, 0, RETRO_HEIGHT - 28, ColorTextGreen);
}

// The point lights on their paths about the world's centre
static void PlacePointLights(double time)
{
	vec3 center = WorldCenter();
	float angle = fmod(time * POINT_LIGHT_RATE, 2 * M_PI);
	PointLightPosition[LIGHT_GREEN] = { center.x - POINT_LIGHT_ORBIT * cosf(angle), PointLightAltitude[LIGHT_GREEN], center.z + POINT_LIGHT_ORBIT * sinf(angle) };
	PointLightPosition[LIGHT_RED] = { center.x - POINT_LIGHT2_ORBIT * cosf(-2 * angle), PointLightAltitude[LIGHT_RED], center.z + POINT_LIGHT2_ORBIT * sinf(-2 * angle) };
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
				int green = light.tint[LIGHT_GREEN] * (TINT_LEVELS - 1) + 0.5f;
				int red = light.tint[LIGHT_RED] * (TINT_LEVELS - 1) + 0.5f;
				LightWeight[c][green][red]++;
			}
		}
	}

	// The ground's colours, as often as its texels have them under the
	// lightmaps, under those lights, then the fan's grey with less say, since
	// it covers less of the screen
	float texels[RETRO_COLORS] = {};
	for (int lightmap = 0; lightmap < LIGHTMAPS; lightmap++) {
		const unsigned char *level = RETRO_ImageData(ASSET_LIGHTMAPS) + lightmap * TEXTURE_SIZE * TEXTURE_SIZE;
		for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) texels[LightmapTable[Sandstone[i]][level[i]]] += 1.0f / LIGHTMAPS;
	}
	RETRO_AddShadeTableColors(&Histogram, RETRO_ImagePalette(ASSET_TERRAIN), texels, LightTable(), &LightWeight[0][0][0], Modulate);
	float greytexels = TEXTURE_SIZE * TEXTURE_SIZE; // as many as the ground's
	RETRO_AddShadeTableColors(&Histogram, &Grey, &greytexels, GreyLightTable(), &LightWeight[0][0][0], Modulate, OBJECT_WEIGHT);

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
		bool on = !(PointLight[LIGHT_GREEN] && PointLight[LIGHT_RED]);
		PointLight[LIGHT_GREEN] = PointLight[LIGHT_RED] = on;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_G)) PointLight[LIGHT_GREEN] = !PointLight[LIGHT_GREEN];
	if (RETRO_KeyPressed(SDL_SCANCODE_R)) PointLight[LIGHT_RED] = !PointLight[LIGHT_RED];
	if (RETRO_KeyPressed(SDL_SCANCODE_H)) Help = !Help;
	float raise = POINT_LIGHT_RAISE * deltatime;
	if (RETRO_KeyState(SDL_SCANCODE_1)) PointLightAltitude[LIGHT_GREEN] -= raise;
	if (RETRO_KeyState(SDL_SCANCODE_2)) PointLightAltitude[LIGHT_GREEN] += raise;
	if (RETRO_KeyState(SDL_SCANCODE_3)) PointLightAltitude[LIGHT_RED] -= raise;
	if (RETRO_KeyState(SDL_SCANCODE_4)) PointLightAltitude[LIGHT_RED] += raise;
	for (float &altitude : PointLightAltitude) {
		altitude = MIN(MAX(altitude, 0.0f), POINT_LIGHT_TOP);
	}

	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	View = mesh.basis;

	PlacePointLights(time);
	vec3 center = WorldCenter();

	// The object over the centre, spinning. The world is the mirror of the
	// book's, so it turns the other way round y to look the same
	float ay = fmod(time * OBJECT_RATE, 2 * M_PI);
	mat3 spin = rotateY(-ay);
	vec3 object = { center.x, OBJECT_ALTITUDE, center.z };

	// The fan's shadow at the fan's turn
	int lightmap = (int)(ay / LIGHTMAP_TURN + 0.5f) % LIGHTMAPS;
	if (lightmap != AppliedLightmap) {
		ApplyLightmap(lightmap);
		AppliedLightmap = lightmap;
	}

	// The sky, and a band of ground colour below it for wherever the
	// landscape does not reach
	RETRO_Clear(ColorSky);
	RETRO_DrawRectangle(0, (int)(RETRO_HEIGHT * 0.38f), RETRO_WIDTH - 1, RETRO_HEIGHT - 1, ColorGround);
	RETRO_ClearDepthBuffer();
	DrawTerrain(mesh);
	DrawModel(Models[MODEL_FAN], spin, object, OBJECT_SCALE, GREY, 0, true);
	DrawModel(Models[MODEL_CUBE], identity(), PointLightPosition[LIGHT_GREEN], LIGHT_OBJECT_SCALE, NO_TEXTURE, ColorCubeGreen, false);
	DrawModel(Models[MODEL_CUBE], identity(), PointLightPosition[LIGHT_RED], LIGHT_OBJECT_SCALE, NO_TEXTURE, ColorCubeRed, false);

	DrawText();
}

void DEMO_Initialize(void)
{
	// The ground's texture, in a palette of its own, and its lightmaps
	RETRO_LoadImage("assets/shadow_terrain2_256x256.pcx");
	RETRO_LoadImage("assets/shadow_fanlight_256x2304.pcx"); // each index its level
	RETRO_SetTerrain(16, 16, TERRAIN_SCALE, FlatGround, NULL, false);

	Models[MODEL_FAN] = RETRO_Load3DModel("assets/shadow_fan.obj");
	Models[MODEL_CUBE] = RETRO_Load3DModel("assets/shadow_cube.obj");

	// The ground's texture at every lightmap level, onto its own palette, and
	// the texture as it was loaded, for laying each lightmap on
	RETRO_CreateShadeTable(RETRO_ImagePalette(ASSET_TERRAIN), RETRO_COLORS, RETRO_COLORS, &LightmapTable[0][0]);
	memcpy(Sandstone, RETRO_ImageData(ASSET_TERRAIN), sizeof(Sandstone));

	FitScreenPalette();
	const RETRO_Palette *palette = ScreenPalette;

	// The colours drawn besides the textures, each held in the palette
	ColorBlack = RETRO_NearestPaletteIndex(RETRO_BLACK, palette);
	ColorTextGreen = RETRO_NearestPaletteIndex(RETRO_GREEN, palette);
	ColorWhite = RETRO_NearestPaletteIndex(RETRO_WHITE, palette);
	ColorCubeGreen = RETRO_NearestPaletteIndex(CubeGreen, palette);
	ColorCubeRed = RETRO_NearestPaletteIndex(CubeRed, palette);
	ColorSky = RETRO_NearestPaletteIndex(Sky, palette);
	ColorGround = RETRO_NearestPaletteIndex(Ground, palette);

	// The ground's light table, from its texture's own colours to the
	// screen's, and the fan's grey's
	RETRO_CreateShadeTable(RETRO_ImagePalette(ASSET_TERRAIN), palette, LightTable(), Modulate);
	RETRO_CreateShadeTable(&Grey, palette, GreyLightTable(), Modulate);

	// The lens: 90 degrees across, square pixels, pitched by the jeep
	RETRO_TerrainView.focalx = RETRO_WIDTH / 2.0f;
	RETRO_TerrainView.focaly = RETRO_WIDTH / 2.0f;
	RETRO_TerrainView.step = 1;
	RETRO_TerrainView.distance = RETRO_Terrain.width * 3 / 2;
	RETRO_TerrainView.nearplane = WORLD(10);

	// The camera starts 500 up and 400 short of the centre, looking at it,
	// and falls onto the ground
	vec3 center = WorldCenter();
	RETRO_Camera.x = center.x;
	RETRO_Camera.z = center.z - WORLD(400);
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
