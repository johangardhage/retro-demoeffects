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
#define POINT_LIGHT_ORANGE_KL 0.001f // attenuation per world unit
#define POINT_LIGHT_YELLOW_KL 0.002f
#define POINT_LIGHT_RATE radians(30) // radians a second round the world
#define POINT_LIGHT_ORANGE_ORBIT WORLD(500) // radius about the world's center
#define POINT_LIGHT_YELLOW_ORBIT WORLD(200) // twice as fast the other way
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
enum { MODEL_ROBOT, MODEL_CUBE, MODELS };
enum { LIGHT_ORANGE, LIGHT_YELLOW, POINT_LIGHTS, LIGHT_SUN = POINT_LIGHTS, LIGHTS };

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
#define ANIMATIONS (int)(sizeof(Animations) / sizeof(Animations[0]))

static unsigned char LightTableData[TEXTURES][RETRO_COLORS][LIGHT_LEVELS][TINT_LEVELS][TINT_LEVELS];
static unsigned char ShadowTable[RETRO_COLORS];
static unsigned char ColorBlack, ColorTextGreen, ColorWhite, ColorCubeRed, ColorCubeYellow;
static Model3D *Models[MODELS];

// The point lights, placed each frame, and the sun, the infinite light,
// toward it. All are in the world, in map cells, where a point light d cells
// away dims as POINT_LIGHT / (kl TO_WORLD(d)). The point lights shine orange
// and yellow, and the sun white, over the ambient light. The keys switch
// them on and off in the rig
static RETRO_Lighting Lights = { LIGHTS, {
	{ {}, POINT_LIGHT, true, {}, 0, POINT_LIGHT_ORANGE_KL * TO_WORLD(1), { 1, 128 / 255.0f, 0 } },
	{ {}, POINT_LIGHT, true, {}, 0, POINT_LIGHT_YELLOW_KL * TO_WORLD(1), { 1, 1, 0 } },
	{ normalize(vec3{ 1, 1, -1 }), INFINITE_LIGHT },
}, AMBIENT_LIGHT };
static_assert(LIGHTS <= RETRO_MAX_LIGHTS, "The point lights and the sun share one rig");
static const RETRO_Lighting Darkness = {}; // what everything is lit by with the lighting off

// The animation playing, and where it has got to
static int CurrentAnimation;
static bool Looped = true;
static float AnimationFrame;

// The switches
static bool Lighting = true;
static bool Help = true;

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
	RETRO_BlendModelPoses(animation.first + (int)frame, animation.first + next, fract(frame), Models[MODEL_ROBOT]);
	RETRO_InitializeFaceNormals(Models[MODEL_ROBOT]);
	RETRO_InitializeVertexNormals(Models[MODEL_ROBOT]);
	AnimationFrame = animation.first + frame;
}

// The sunset, which drifts to the left, and turns the other way to the camera
// The sky drifts, and turns with the heading. It rides up and down with the
// horizon as the vehicle's nose tips, from where it sits at the resting
// pitch, the image's top or bottom row repeated over whatever it uncovers.
static void DrawSky(double time)
{
	int scroll = WRAP((float)(time * SKY_DRIFT - RETRO_TerrainCamera.heading * SKY_TURN), RETRO_WIDTH);
	float tip = tanf(RETRO_Vehicle.restpitch) - tanf(RETRO_Vehicle.pitch);
	int rise = (int)lroundf(View.lens.focaly * tip);
	const unsigned char *sky = RETRO_ImageData(ASSET_SKY);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		const unsigned char *row = &sky[CLAMPHEIGHT(y - rise) * RETRO_WIDTH];
		unsigned char *screen = RETRO_FrameBuffer() + y * RETRO_WIDTH;
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
	vec3 center = RETRO_TerrainCenter();
	// Each line within the screen's 40 columns
	snprintf(text, sizeof(text), "CAM [%5.0f,%5.0f,%5.0f] CELL [%d, %d]\nLighting [%s]: Amb=%d Inf=%d O=%d Y=%d\nAnim[%d]=%s Frm=%.1f",
			 TO_WORLD(RETRO_TerrainCamera.x - center.x), TO_WORLD(RETRO_TerrainCamera.height), TO_WORLD(RETRO_TerrainCamera.z - center.z), (int)floorf(RETRO_TerrainCamera.x), (int)floorf(RETRO_TerrainCamera.z),
			 Lighting ? "ON" : "OFF", Lights.ambient > 0, Lights.light[LIGHT_SUN].on, Lights.light[LIGHT_ORANGE].on, Lights.light[LIGHT_YELLOW].on,
			 CurrentAnimation, Animations[CurrentAnimation].name, AnimationFrame);
	RETRO_PutString(text, 0, RETRO_HEIGHT - 28, ColorTextGreen);
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	RETRO_UpdateTerrainVehicle(time.delta);
}

void DEMO_Render(RETRO_Time time)
{
	static double animationstart; // when the animation playing began

	if (RETRO_KeyPressed(SDL_SCANCODE_L)) Lighting = !Lighting;
	if (RETRO_KeyPressed(SDL_SCANCODE_A)) Lights.ambient = Lights.ambient > 0 ? 0 : AMBIENT_LIGHT;
	if (RETRO_KeyPressed(SDL_SCANCODE_I)) Lights.light[LIGHT_SUN].on = !Lights.light[LIGHT_SUN].on;
	if (RETRO_KeyPressed(SDL_SCANCODE_P)) {
		// Both on unless both already are
		bool on = !(Lights.light[LIGHT_ORANGE].on && Lights.light[LIGHT_YELLOW].on);
		Lights.light[LIGHT_ORANGE].on = Lights.light[LIGHT_YELLOW].on = on;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_O)) Lights.light[LIGHT_ORANGE].on = !Lights.light[LIGHT_ORANGE].on;
	if (RETRO_KeyPressed(SDL_SCANCODE_Y)) Lights.light[LIGHT_YELLOW].on = !Lights.light[LIGHT_YELLOW].on;
	if (RETRO_KeyPressed(SDL_SCANCODE_H)) Help = !Help;
	bool previous = RETRO_KeyPressed(SDL_SCANCODE_1);
	bool next = RETRO_KeyPressed(SDL_SCANCODE_2);
	bool once = RETRO_KeyPressed(SDL_SCANCODE_3);
	bool looped = RETRO_KeyPressed(SDL_SCANCODE_4);
	if (previous) CurrentAnimation = (CurrentAnimation + ANIMATIONS - 1) % ANIMATIONS;
	if (next) CurrentAnimation = (CurrentAnimation + 1) % ANIMATIONS;
	if (previous || next || once || looped) {
		Looped = looped;
		animationstart = time.total;
	}

	RETRO_TerrainMesh mesh = RETRO_BuildTerrainMesh();
	View = mesh.camera;

	// The point lights on their paths about the world's center
	vec3 center = RETRO_TerrainCenter();
	float angle = fmod(time.total * POINT_LIGHT_RATE, 2 * M_PI);
	Lights.light[LIGHT_ORANGE].position = { center.x - POINT_LIGHT_ORANGE_ORBIT * cosf(angle), POINT_LIGHT_ALTITUDE, center.z + POINT_LIGHT_ORANGE_ORBIT * sinf(angle) };
	Lights.light[LIGHT_YELLOW].position = { center.x - POINT_LIGHT_YELLOW_ORBIT * cosf(-2 * angle), POINT_LIGHT_ALTITUDE, center.z + POINT_LIGHT_YELLOW_ORBIT * sinf(-2 * angle) };

	// The mechs, side by side at the world's center
	PoseRobot(time.total - animationstart);
	vec3 robots[] = { { center.x, ROBOT_ALTITUDE, center.z }, { center.x, ROBOT_ALTITUDE, center.z + ROBOT_SPACING } };

	DrawSky(time.total);
	RETRO_ClearDepthBuffer();
	RETRO_DrawTerrainMesh(mesh, TerrainVertex, RETRO_ImageData(ASSET_TERRAIN), TEXTURE_SIZE, TEXTURE_SIZE, LightTable(ASSET_TERRAIN));
	for (vec3 robot : robots) {
		RETRO_DrawTerrainModel(Models[MODEL_ROBOT], identity(), robot, ROBOT_SCALE, Lighting ? Lights : Darkness, RETRO_ImageData(ASSET_SKIN), TEXTURE_SIZE, TEXTURE_SIZE, LightTable(ASSET_SKIN), &View);
	}
	RETRO_DrawTerrainFlatModel(Models[MODEL_CUBE], identity(), Lights.light[LIGHT_ORANGE].position, LIGHT_OBJECT_SCALE, Lighting ? ColorCubeRed : ColorBlack, &View);
	RETRO_DrawTerrainFlatModel(Models[MODEL_CUBE], identity(), Lights.light[LIGHT_YELLOW].position, LIGHT_OBJECT_SCALE, Lighting ? ColorCubeYellow : ColorBlack, &View);

	// A shadow of both from each point light that is on, in one pass, so that
	// where the two overlap the ground is darkened once, as under one shadow
	for (int i = 0; i < POINT_LIGHTS; i++) {
		if (!Lighting || !Lights.light[i].on) continue;
		bool newpass = true;
		for (vec3 robot : robots) {
			RETRO_DrawTerrainShadow(Models[MODEL_ROBOT], identity(), robot, ROBOT_SCALE, Lights.light[i].position, SHADOW_PLANE, ShadowTable, &View, newpass);
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
		RETRO_CreateShadeTable(RETRO_ImagePalette(texture), palette, LightTable(texture), RETRO_TintColor);
	}

	// Every entry, since the shadow plane reaches past the ground's edge,
	// over the sunset
	RETRO_CreateShadeTable(palette, RETRO_COLORS, 1, ShadowTable, SHADOW_LIGHT);

	// The lens: 90 degrees across, square pixels, pitched by the jeep
	RETRO_TerrainCamera.lens.focalx = RETRO_WIDTH / 2.0f;
	RETRO_TerrainCamera.lens.focaly = RETRO_WIDTH / 2.0f;
	RETRO_TerrainCamera.lens.center.y = RETRO_HEIGHT / 2.0f; // the vehicle pitches about the middle
	RETRO_TerrainView.step = 1;
	RETRO_TerrainView.distance = RETRO_Terrain.width * 3 / 2;
	RETRO_TerrainCamera.lens.nearplane = WORLD(10);

	// The camera starts 500 up and 400 in front of the mechs, halfway between
	// them and looking them in the face, along +x, and falls onto the ground
	vec3 center = RETRO_TerrainCenter();
	RETRO_TerrainCamera.x = center.x - WORLD(400);
	RETRO_TerrainCamera.z = center.z + ROBOT_SPACING / 2;
	RETRO_TerrainCamera.height = WORLD(500);
	RETRO_TerrainCamera.heading = 3 * M_PI / 2;

	// The hovering jeep the camera rides
	RETRO_Vehicle.acceleration = WORLD(900);
	RETRO_Vehicle.friction = WORLD(225);
	RETRO_Vehicle.maxspeed = WORLD(600);
	RETRO_Vehicle.turnspeed = radians(150);
	RETRO_Vehicle.gravity = WORLD(360);
	RETRO_Vehicle.spring = 22.5f;
	RETRO_Vehicle.lift = 10.7f; // -ln(0.7) * 30
	RETRO_Vehicle.clearance = WORLD(150);
	RETRO_Vehicle.floor = WORLD(50);
	RETRO_Vehicle.restpitch = RETRO_Vehicle.pitch = radians(10);
	RETRO_Vehicle.pitchkick = radians(0.6f) / WORLD(1);
	RETRO_Vehicle.pitchreturn = radians(10);

	RETRO_SetFont(RETRO_FONT_VGA_8X8);
}
