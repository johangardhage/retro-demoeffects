//
// Robots
//
// Two mechs stand side by side on a lit, textured landscape to drive over,
// facing the camera as it starts, and play the animations of a Quake II player
// model: standing, running, attacking, flinching, jumping, taunting, crouching
// and dying. Four lights shine on them - ambient, a sun, and an orange and a
// yellow point light circling the world - with a cube marking each point
// light, and a sunset behind it all that drifts, and scrolls as the camera
// turns.
//
// The world is measured in units of its own, 4000 across the 40 by 40 height
// map; WORLD() takes those to map cells.
//
// The mech is one mesh posed from the frames of its model: each animation is a
// run of them, played at a rate of its own, and a pose between two frames mixes
// them. A looped animation closes from its last frame back into its first; one
// played once holds its last. Both mechs take the same pose.
//
// The point lights have a hue, so a vertex's light is three numbers: a neutral
// level, and how much further red and green reach, since both lights are
// mostly red and neither has any blue. All three are interpolated across a
// face and looked up, with the texel, in a table per texture that takes the
// texture's own palette onto the screen's:
//
//   table[t][n][r][g] = nearest(texture[t] · (min(n + r, 1), min(n + g, 1), n))
//
// The ground and the mechs are lit at their corners. A mech's corner faces the
// average of the faces that meet there, each weighted by its area, taken anew
// for every pose, so the light runs smoothly over the body as it moves.
//
// Each point light throws a shadow of each mech: every corner of it is
// followed along its ray from the light to a level plane just above the plain
// at the world's center, where a corner at height h under a light at height l
// lands (l - plane) / (l - h) as far from the light as it started. Only the
// faces turned toward the light are drawn there. The plane is flat, so the
// shadow does not climb a hill: the hill rises through it and hides it. It is
// depth tested but writes no depth, and darkens what is under it through a
// table, in one pass for both mechs that darkens each pixel once however many
// of their faces cover it. Where the two lights' shadows cross it is darker
// still, and with a point light out there is none from it.
//
// Faces are cut at the near plane in the camera's frame before they are
// projected, so one reaching from behind the camera leaves no hole.
//
// Up/Down drive, Left/Right turn. L all lighting, A ambient, I sun, P both
// point lights, O and Y the orange and the yellow one, 1 and 2 play the
// previous and the next animation once, 3 plays this one once more and 4
// loops it, H help.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
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
#define POINT_LIGHT_KL 0.001f // orange, attenuation per world unit
#define POINT_LIGHT2_KL 0.002f // yellow
#define POINT_LIGHT_RATE (30 * DEG2RAD) // radians a second round the world
#define POINT_LIGHT_ORBIT WORLD(500) // orange, radius about the world's center
#define POINT_LIGHT2_ORBIT WORLD(200) // yellow, twice as fast the other way
#define POINT_LIGHT_ALTITUDE WORLD(500)
#define LIGHT_LEVELS 32 // neutral light, in the tables
#define TINT_LEVELS 8 // and red and green beyond it

#define ROBOT_SCALE WORLD(4) // world units per model unit
#define ROBOT_ALTITUDE WORLD(100) // height of the model's origin
#define ROBOT_SPACING WORLD(200) // the second stands this much further along z
#define LIGHT_OBJECT_SCALE WORLD(10)

#define SHADOW_PLANE WORLD(10) // height of the plane the shadows lie on
#define SHADOW_LIGHT (5 / 7.0f) // fraction of the light left in the shadow

// The sunset is 800 pixels across in the book, and moves one of them a frame
// at 30 frames a second. It goes round once a turn of the camera, so it is
// where it was whenever the camera faces the same way again, heading wrapping
// or not
#define SKY_DRIFT (30 * RETRO_WIDTH / 800.0f) // pixels a second
#define SKY_TURN (RETRO_WIDTH / (2 * M_PI)) // pixels a radian of turn

#define TEXTURE_SIZE 256 // every texture, on a side

enum { ASSET_SKY, ASSET_TERRAIN, ASSET_SKIN, ASSET_HEIGHTMAP };
#define TEXTURES (ASSET_HEIGHTMAP - ASSET_TERRAIN)
#define NO_TEXTURE -1
enum { MODEL_ROBOT, MODEL_CUBE, MODELS };
enum { LIGHT_ORANGE, LIGHT_YELLOW, POINT_LIGHTS };
enum { TINT_RED, TINT_GREEN, TINTS };

// A run of the model's frames, and how many of them it plays a second
struct Animation {
	const char *name;
	int first, last;
	float rate;
};

static const Animation Animations[] = {
	{ "STANDING_IDLE", 0, 39, 15 },
	{ "RUN", 40, 45, 7.5f },
	{ "ATTACK", 46, 53, 15 },
	{ "PAIN_1", 54, 57, 15 },
	{ "PAIN_2", 58, 61, 15 },
	{ "PAIN_3", 62, 65, 15 },
	{ "JUMP", 66, 71, 15 },
	{ "FLIP", 72, 83, 15 },
	{ "SALUTE", 84, 94, 15 },
	{ "TAUNT", 95, 111, 15 },
	{ "WAVE", 112, 122, 15 },
	{ "POINT", 123, 134, 15 },
	{ "CROUCH_STAND", 135, 153, 15 },
	{ "CROUCH_WALK", 154, 159, 15 },
	{ "CROUCH_ATTACK", 160, 168, 15 },
	{ "CROUCH_PAIN", 169, 172, 15 },
	{ "CROUCH_DEATH", 173, 177, 7.5f },
	{ "DEATH_BACK", 178, 183, 7.5f },
	{ "DEATH_FORWARD", 184, 189, 7.5f },
	{ "DEATH_SLOW", 190, 197, 7.5f }
};
static constexpr int ANIMATIONS = sizeof(Animations) / sizeof(Animations[0]);

// Light as summed, reduced to the numbers that differ
struct Light {
	float neutral;		// all three channels, capped at full
	float tint[TINTS];	// and how much further red and green reach
};
static_assert(TINTS <= RETRO_MAX_TINTS, "Red and green each need a tint of their own");

static unsigned char LightTableData[TEXTURES][RETRO_COLORS][LIGHT_LEVELS][TINT_LEVELS][TINT_LEVELS];
static unsigned char ShadowTable[RETRO_COLORS];
static unsigned char ColorBlack, ColorTextGreen, ColorWhite, ColorCubeRed, ColorCubeYellow;
static Model3D *Models[MODELS];
static const vec3 Sun = normalize(vec3{ 1, 1, -1 }); // the infinite light, toward it

static const float PointLightKL[POINT_LIGHTS] = { POINT_LIGHT_KL, POINT_LIGHT2_KL };
static const float PointLightTint[POINT_LIGHTS][TINTS] = { { 1, 128 / 255.0f }, { 1, 1 } }; // orange, and yellow
static vec3 PointLightPosition[POINT_LIGHTS];

// The animation playing, since when, and where it has got to
static int CurrentAnimation;
static bool Looped = true;
static double AnimationStart;
static float AnimationFrame;

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

// A color under the light: the neutral level as the shade, and red and green
// beyond it as the tints
static RETRO_Palette Modulate(RETRO_Palette color, float neutral, const float *tint)
{
	float red = MIN(neutral + tint[TINT_RED], 1.0f);
	float green = MIN(neutral + tint[TINT_GREEN], 1.0f);
	return { (unsigned char)(color.r * red), (unsigned char)(color.g * green), (unsigned char)(color.b * neutral) };
}

// A point at p facing n, lit
static Light LightPoint(vec3 p, vec3 n)
{
	Light light = {};
	if (!Lighting) return light;

	if (AmbientLight) light.neutral += AMBIENT_LIGHT;
	if (InfiniteLight) light.neutral += INFINITE_LIGHT * MAX(dot(n, Sun), 0.0f);
	light.neutral = MIN(light.neutral, 1.0f);
	float sum[TINTS] = {};
	for (int i = 0; i < POINT_LIGHTS; i++) {
		if (!PointLight[i]) continue;
		float term = PointLightTerm(p, n, PointLightPosition[i], PointLightKL[i]);
		for (int c = 0; c < TINTS; c++) {
			sum[c] += term * PointLightTint[i][c];
		}
	}
	for (int c = 0; c < TINTS; c++) {
		light.tint[c] = MIN(light.neutral + sum[c], 1.0f) - light.neutral;
	}
	return light;
}

// A world point in the camera's frame, carrying its texture coordinates and
// its light as table levels: neutral as the shade, red and green as the tints
static RETRO_TerrainVertex MakeVertex(vec3 p, vec2 uv, Light light)
{
	RETRO_TerrainVertex vertex = {};
	vertex.eye = RETRO_TerrainPointEye(p.x, p.z, p.y, View);
	vertex.uv = uv;
	vertex.c = light.neutral * (LIGHT_LEVELS - 1) + 0.5f;
	for (int c = 0; c < TINTS; c++) {
		vertex.tint[c] = light.tint[c] * (TINT_LEVELS - 1) + 0.5f;
	}
	return vertex;
}

// A texture's light table, from its own colors to the screen's
static RETRO_ShadeTable LightTable(int texture)
{
	return { &LightTableData[texture - ASSET_TERRAIN][0][0][0][0], RETRO_COLORS, LIGHT_LEVELS, { TINT_LEVELS, TINT_LEVELS } };
}

//
// One polygon, through the near plane and onto the screen: textured and lit
// when there is a texture, a flat color when there is not. Flat colors
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
	return MakeVertex(p, uv, LightPoint(p, n));
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

// One of the models at a position and scale, lit at its corners
static void DrawModel(const Model3D *model, vec3 position, float scale, int texture, unsigned char color, bool lit)
{
	for (int i = 0; i < model->faces; i++) {
		const Face *face = &model->face[i];
		RETRO_TerrainVertex polygon[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			vec3 p = position + model->vertex[face->vertex[j]].pos * scale;
			Light light = lit ? LightPoint(p, model->normal[face->vertexnormal[j]].dir) : Light{ 1, {} };
			polygon[j] = MakeVertex(p, model->uv[face->uv[j]], light);
		}
		DrawPolygon(polygon, face->vertices, texture, color);
	}
}

//
// The shadow of a model at a position and scale, cast by a point light onto
// the shadow plane, in the pass newpass begins or the one before it. None
// when a corner is level with the light or above it, since its ray never
// comes down to the plane
//
static void DrawShadow(const Model3D *model, vec3 position, float scale, vec3 light, bool &newpass)
{
	vec3 flat[RETRO_MAX_VERTICES];
	for (int i = 0; i < model->vertices; i++) {
		vec3 p = position + model->vertex[i].pos * scale;
		if (p.y >= light.y) return;
		flat[i] = light + (p - light) * ((light.y - SHADOW_PLANE) / (light.y - p.y));
	}

	for (int i = 0; i < model->faces; i++) {
		const Face *face = &model->face[i];
		vec3 first = position + model->vertex[face->vertex[0]].pos * scale;
		if (dot(face->facenormal.dir, light - first) <= 0) continue;

		RETRO_TerrainVertex shadow[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			shadow[j] = MakeVertex(flat[face->vertex[j]], { 0, 0 }, { 1, {} });
		}
		PolygonPoint polygon[RETRO_TERRAIN_MAX_POLYGON + 1];
		int points = RETRO_ClipProjectTerrainPolygon(shadow, face->vertices, polygon);
		RETRO_DrawRemapPolygon(polygon, points, ShadowTable, newpass);
		newpass = false;
	}
}

//
// The mechs' pose, time seconds into the animation: a looped one runs on
// through its last frame back into its first, and one played once stops on
// its last. The faces and corners are turned with the pose, and lit from
// where they face
//
static void PoseRobot(double time)
{
	const Animation &animation = Animations[CurrentAnimation];
	int frames = animation.last - animation.first + 1;
	float frame;
	int next;
	if (Looped) {
		frame = fmod(time * animation.rate, frames);
		next = ((int)frame + 1) % frames;
	} else {
		frame = MIN(time * animation.rate, frames - 1.0);
		next = MIN((int)frame + 1, frames - 1);
	}
	RETRO_BlendModelPoses(animation.first + (int)frame, animation.first + next, frame - (int)frame, Models[MODEL_ROBOT]);
	RETRO_InitializeFaceNormals(Models[MODEL_ROBOT]);
	RETRO_InitializeVertexNormals(Models[MODEL_ROBOT]);
	AnimationFrame = animation.first + frame;
}

// The sunset, which drifts to the left, and turns the other way to the camera
static void DrawSky(double time)
{
	int scroll = WRAP((float)(time * SKY_DRIFT - RETRO_Camera.heading * SKY_TURN), RETRO_WIDTH);
	const unsigned char *sky = RETRO_ImageData(ASSET_SKY);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		const unsigned char *row = &sky[y * RETRO_WIDTH];
		unsigned char *screen = &RETRO.framebuffer[y * RETRO_WIDTH];
		memcpy(screen, row + scroll, RETRO_WIDTH - scroll);
		memcpy(screen + RETRO_WIDTH - scroll, row, scroll);
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
						"<O>/<Y> Toggle orange/yellow light\n"
						"<1>/<2> Previous/next animation\n"
						"<3>/<4> Play animation once/looped\n"
						"<H> Toggle Help\n"
						"<Q> Exit demo", 0, 14, ColorWhite);
	}

	// CELL is the cell the camera is over
	vec3 center = WorldCenter();
	// Each line within the screen's 40 columns
	snprintf(text, sizeof(text), "CAM [%5.0f,%5.0f,%5.0f] CELL [%d, %d]\nLighting [%s]: Amb=%d Inf=%d O=%d Y=%d\nAnim[%d]=%s Frm=%.1f",
			 TO_WORLD(RETRO_Camera.x - center.x), TO_WORLD(RETRO_Camera.height), TO_WORLD(RETRO_Camera.z - center.z), (int)floorf(RETRO_Camera.x), (int)floorf(RETRO_Camera.z),
			 Lighting ? "ON" : "OFF", AmbientLight, InfiniteLight, PointLight[LIGHT_ORANGE], PointLight[LIGHT_YELLOW],
			 CurrentAnimation, Animations[CurrentAnimation].name, AnimationFrame);
	RETRO_PutString(text, 0, RETRO_HEIGHT - 28, ColorTextGreen);
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
		bool on = !(PointLight[LIGHT_ORANGE] && PointLight[LIGHT_YELLOW]);
		PointLight[LIGHT_ORANGE] = PointLight[LIGHT_YELLOW] = on;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_O)) PointLight[LIGHT_ORANGE] = !PointLight[LIGHT_ORANGE];
	if (RETRO_KeyPressed(SDL_SCANCODE_Y)) PointLight[LIGHT_YELLOW] = !PointLight[LIGHT_YELLOW];
	if (RETRO_KeyPressed(SDL_SCANCODE_H)) Help = !Help;
	bool previous = RETRO_KeyPressed(SDL_SCANCODE_1);
	bool next = RETRO_KeyPressed(SDL_SCANCODE_2);
	bool once = RETRO_KeyPressed(SDL_SCANCODE_3);
	bool looped = RETRO_KeyPressed(SDL_SCANCODE_4);
	if (previous) CurrentAnimation = (CurrentAnimation + ANIMATIONS - 1) % ANIMATIONS;
	if (next) CurrentAnimation = (CurrentAnimation + 1) % ANIMATIONS;
	if (previous || next || once || looped) {
		Looped = looped;
		AnimationStart = time;
	}

	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	View = mesh.basis;

	// The point lights on their paths about the world's center
	vec3 center = WorldCenter();
	float angle = fmod(time * POINT_LIGHT_RATE, 2 * M_PI);
	PointLightPosition[LIGHT_ORANGE] = { center.x - POINT_LIGHT_ORBIT * cosf(angle), POINT_LIGHT_ALTITUDE, center.z + POINT_LIGHT_ORBIT * sinf(angle) };
	PointLightPosition[LIGHT_YELLOW] = { center.x - POINT_LIGHT2_ORBIT * cosf(-2 * angle), POINT_LIGHT_ALTITUDE, center.z + POINT_LIGHT2_ORBIT * sinf(-2 * angle) };

	// The mechs, side by side at the world's center
	PoseRobot(time - AnimationStart);
	vec3 robots[] = { { center.x, ROBOT_ALTITUDE, center.z }, { center.x, ROBOT_ALTITUDE, center.z + ROBOT_SPACING } };

	DrawSky(time);
	RETRO_ClearDepthBuffer();
	DrawTerrain(mesh);
	for (vec3 robot : robots) {
		DrawModel(Models[MODEL_ROBOT], robot, ROBOT_SCALE, ASSET_SKIN, 0, true);
	}
	DrawModel(Models[MODEL_CUBE], PointLightPosition[LIGHT_ORANGE], LIGHT_OBJECT_SCALE, NO_TEXTURE, ColorCubeRed, false);
	DrawModel(Models[MODEL_CUBE], PointLightPosition[LIGHT_YELLOW], LIGHT_OBJECT_SCALE, NO_TEXTURE, ColorCubeYellow, false);

	// A shadow of both from each point light that is on, in one pass, so that
	// where the two overlap the ground is darkened once, as under one shadow
	for (int i = 0; i < POINT_LIGHTS; i++) {
		if (!Lighting || !PointLight[i]) continue;
		bool newpass = true;
		for (vec3 robot : robots) {
			DrawShadow(Models[MODEL_ROBOT], robot, ROBOT_SCALE, PointLightPosition[i], newpass);
		}
	}

	DrawText();
}

void DEMO_Initialize(void)
{
	// The sunset, which carries the screen's palette, then the textures, each
	// in a palette of its own
	RETRO_LoadImage("assets/robots_sky_320x240.pcx", true);
	RETRO_LoadImage("assets/shadow_terrain_256x256.pcx");
	RETRO_LoadImage("assets/robots_skin_256x256.pcx");
	RETRO_Image *heightmap = RETRO_LoadImage("assets/shadow_height_40x40.pcx");
	RETRO_SetTerrain(heightmap->width, heightmap->height, TERRAIN_SCALE, heightmap->data, NULL, false);

	Models[MODEL_ROBOT] = RETRO_LoadMD2Model("assets/robots_blade.md2");
	Models[MODEL_CUBE] = RETRO_Load3DModel("assets/shadow_cube.obj");

	// The model faces +x, and the book's mechs face -x: turn every frame of
	// it half about y
	Model3D *robot = Models[MODEL_ROBOT];
	for (int i = 0; i < robot->frames * robot->vertices; i++) {
		robot->frame[i * 3] = -robot->frame[i * 3];
		robot->frame[i * 3 + 2] = -robot->frame[i * 3 + 2];
	}

	const RETRO_Palette *palette = RETRO_ImagePalette(ASSET_SKY);

	// The colors drawn besides the textures. The palette is fitted to the
	// scene, and holds the read-out's green and white exactly; the rest are
	// each their nearest entry there rather than one set aside
	ColorBlack = RETRO_NearestPaletteIndex(RETRO_BLACK, palette);
	ColorTextGreen = RETRO_NearestPaletteIndex(RETRO_GREEN, palette);
	ColorWhite = RETRO_NearestPaletteIndex(RETRO_WHITE, palette);
	ColorCubeRed = RETRO_NearestPaletteIndex(RETRO_Palette{ 255, 0, 0 }, palette);
	ColorCubeYellow = RETRO_NearestPaletteIndex(RETRO_Palette{ 251, 255, 10 }, palette);

	// A light table per texture, from its own colors to the screen's
	for (int texture = ASSET_TERRAIN; texture < ASSET_TERRAIN + TEXTURES; texture++) {
		RETRO_CreateShadeTable(RETRO_ImagePalette(texture), palette, LightTable(texture), Modulate);
	}

	// Every entry, since the shadow plane reaches past the ground's edge,
	// over the sunset
	RETRO_CreateShadeTable(palette, RETRO_COLORS, 1, ShadowTable, SHADOW_LIGHT);

	// The lens: 90 degrees across, square pixels, pitched by the jeep
	RETRO_TerrainView.focalx = RETRO_WIDTH / 2.0f;
	RETRO_TerrainView.focaly = RETRO_WIDTH / 2.0f;
	RETRO_TerrainView.step = 1;
	RETRO_TerrainView.distance = RETRO_Terrain.width * 3 / 2;
	RETRO_TerrainView.nearplane = WORLD(10);

	// The camera starts 500 up and 400 in front of the mechs, halfway between
	// them and looking them in the face, along +x, and falls onto the ground
	vec3 center = WorldCenter();
	RETRO_Camera.x = center.x - WORLD(400);
	RETRO_Camera.z = center.z + ROBOT_SPACING / 2;
	RETRO_Camera.height = WORLD(500);
	RETRO_Camera.heading = 3 * M_PI / 2;

	// The hovering jeep the camera rides
	RETRO_Vehicle.acceleration = WORLD(900);
	RETRO_Vehicle.friction = WORLD(225);
	RETRO_Vehicle.maxspeed = WORLD(600);
	RETRO_Vehicle.turnspeed = 150 * DEG2RAD;
	RETRO_Vehicle.gravity = WORLD(360);
	RETRO_Vehicle.spring = 22.5f;
	RETRO_Vehicle.lift = 10.7f; // -ln(0.7) * 30
	RETRO_Vehicle.clearance = WORLD(150);
	RETRO_Vehicle.floor = WORLD(50);
	RETRO_Vehicle.restpitch = RETRO_Vehicle.pitch = 10 * DEG2RAD;
	RETRO_Vehicle.pitchkick = 0.6f * DEG2RAD / WORLD(1);
	RETRO_Vehicle.pitchreturn = 10 * DEG2RAD;

	RETRO_SetFont(RETRO_FONT_VGA_8X8);
}
