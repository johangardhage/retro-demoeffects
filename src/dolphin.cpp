//
// Dolphin
//
// A dolphin swims in place while the camera circles it, and a school of fish
// cross the scene behind and in front of it. The meshes are dolphin.md3 and
// fish2.md3, each holding three poses and the normals that go with
// them. Every face of the dolphin carries the
// same three corners of a middle crop of dolphin_256x256.pcx, about u 0.22 to
// 0.80 and v 0.13 to 0.75, which streaks it along the body. The fish's faces
// are all a single texel of it. Everything is lit from the eye through a
// shade table built from the picture's 31 colors.
//
// Each mesh swims by blending between its poses in the order 0, 1, 2, 1, the
// dolphin making SWIM_BLENDS blends over the part and the fish FISH_PACE
// times as many. SWIM_BLENDS is a whole number of orders, so the swim comes
// round to where it started as the part loops. The fish swim in FISH_PHASES
// phases, FISH_PER_PHASE at a time to each in turn. The dolphin starts at the
// start of the order and each phase of fish a blend further around it than
// the one before.
//
// A fish swims SWIM_SPEED along x a second. The fish start queued up behind
// -FISH_FAR, FISH_GAP apart, and swim in one after another and on out of the
// scene, never turning back, so none is ever seen to jump. There are enough
// of them that the queue lasts the whole part. Each fish's height is hashed
// from it, within FISH_SPREAD of the middle, and its depth to one side or the
// other of the dolphin, between FISH_NEAR and FISH_SPREAD, so where a fish is
// follows from the time alone and none swims through the dolphin.
//
// A keyframe table gives the camera's position and angles, and each frame
// mixes the two keyframes it falls between. The part lasts until the last
// keyframe, fading in over the first FADE seconds and out over the last FADE.
// Then it all starts again.
//
// The tables are in the library's axes, x right, y down and z away, with
// angles in degrees. The meshes are stored upright in the same axes, facing
// +x as the Quake models load. Each object is turned to its heading about y,
// the fish swimming toward +x and the dolphin heading into them, and then
// turned and placed by the camera.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromodel.h"
#include "lib/retromatrix.h"
#include "lib/retrorender.h"
#include "lib/retroshadetable.h"

constexpr float FOCAL = 512.0f;            // the eye's distance from the screen, in pixels
constexpr float EYE = 200.0f;              // and from the scene's z = 0, in its units

constexpr int TEXTURE_COLORS = 31;         // colors in dolphin_256x256.pcx
constexpr float SPECULARITY = 0.85f;       // the highlight the shade table ramps up to
constexpr float FALLOFF = 4.0f;            // and how tight it is

constexpr float MD3_SCALE = 32.0f;         // steps per unit both meshes were packed at
constexpr int SWIM_BLENDS = 36;            // blends the dolphin makes over the part, a multiple of SWIM_ORDER
constexpr int SWIM_ORDER = 4;              // blends before the order repeats
constexpr int FISH_PER_PHASE = 2;
constexpr int FISH_PHASES = 4;
constexpr int FISH = 12;                   // enough that the queue lasts until the last keyframe
constexpr int FISH_PACE = 2;               // how much faster the fish blend their poses than the dolphin

constexpr float SWIM_SPEED = 350.0f;       // units a fish swims a second
constexpr int FISH_FAR = 1900;             // how far along -x the first fish starts
constexpr float FISH_GAP = 1000.0f;        // and how far apart along x the fish swim
constexpr int FISH_SPREAD = 500;           // how far either way in y and z the fish swim
constexpr int FISH_NEAR = 250;             // and how near in z, clear of the dolphin
constexpr float FISH_HEADING = 0.0f;       // degrees about y the fish are turned, swimming toward +x
constexpr float DOLPHIN_HEADING = 180.0f;  // and the dolphin, heading into them

constexpr float FADE = 1.8f;                          // seconds the fades take

// Where the camera stands and how it is turned, at a time. Between two
// keyframes both move in a straight line, and the angles are turned through
// as they are written, not by the shortest way round
struct Keyframe {
	float time;
	vec3 position;
	vec3 angles;
};

static const Keyframe Keyframes[] = {
	{ 0.0f, { -1521, 21, 629 }, { 0.1f, -112, -1.8f } },
	{ 3.3f, { -1893, 0, 1067 }, { 0, -122.3f, 0 } },
	{ 6.6f, { -1279, 0, 1222 }, { 0, -136.1f, 0 } },
	{ 9.9f, { -664, 0, 579 }, { 0, -130.3f, 0 } },
	{ 13.2f, { 816, 0, 0 }, { 0, -270, 0 } },
	{ 16.5f, { 1220, -50, 564 }, { 0, -229.7f, 0 } },
	{ 19.8f, { 1066, -94, -1225 }, { 0, -325.6f, 0 } },
	{ 23.1f, { -642, 3, -1036 }, { 0, -408.7f, 0 } },
	{ 26.4f, { -1242, -40, -980 }, { 0, -407.5f, 0 } },
	{ 29.7f, { -2285, 29, 1616 }, { 0, -489, 0 } },
	{ 33.0f, { -4292, 29, 3260 }, { 0, -489, 0 } },
};

constexpr int KEYFRAMES = (int)(sizeof(Keyframes) / sizeof(Keyframes[0]));

// The pose at each place in the swimming order
static const int SwimOrder[SWIM_ORDER] = { 0, 1, 2, 1 };

static Model3D *Dolphin;
static Model3D *Fish;
static unsigned char ShadeTable[RETRO_SHADE_TABLE_SIZE];
static RETRO_Palette Palette[RETRO_COLORS];

// Fish i at a time: how far it has swum, less its place in the queue
static vec3 SwimFish(int i, float time)
{
	float z = mix(FISH_NEAR, FISH_SPREAD, RETRO_HashUnit(i, 0, 1));
	return {
		-FISH_FAR + SWIM_SPEED * time - i * FISH_GAP,
		mix(-FISH_SPREAD, FISH_SPREAD, RETRO_HashUnit(i, 0, 0)),
		RETRO_HashUnit(i, 0, 2) < 0.5f ? -z : z
	};
}

// The model posed blends blends along the swimming order
static void Pose(Model3D *model, float blends)
{
	int blend = (int)blends;
	RETRO_BlendModelPoses(SwimOrder[blend % SWIM_ORDER], SwimOrder[(blend + 1) % SWIM_ORDER], blends - blend, model);
}

// The model as posed, turned heading degrees about y, placed at position in
// the scene, and drawn as the camera, standing at eye and turned by camera,
// sees it
static void Draw(Model3D *model, float heading, vec3 position, vec3 eye, const mat3 &camera)
{
	vec3 seen = camera * (position - eye);
	RETRO_RotateModel(camera * rotateY((float)radians(heading)), model);
	RETRO_TranslateModel(seen.x, seen.y, seen.z, model);

	// A face reaching in front of the screen is dropped whole
	RETRO_ProjectModel(FOCAL / EYE, RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f, model, FOCAL, true);
	RETRO_RenderModel(RETRO_POLY_TEXTURE, RETRO_SHADE_GOURAUD, model, false);
}

void DEMO_Render(RETRO_Time time)
{
	// The two keyframes this moment falls between. The time leads each one,
	// so the keyframes are walked a keyframe apart
	RETRO_KeyframeLoop span = RETRO_LoopKeyframes(time.total, Keyframes, KEYFRAMES, sizeof(Keyframe));
	const Keyframe &from = Keyframes[span.key];
	const Keyframe &to = Keyframes[span.key + 1];

	// Fading in at the start and out at the end
	float now = (float)span.seconds;
	RETRO_Fade(MIN(now, (float)span.length - now) / FADE, Palette);

	// The camera between the two keyframes
	vec3 eye = mix(from.position, to.position, span.t);
	vec3 view = mix(from.angles, to.angles, span.t) * (float)(M_PI / 180);
	mat3 camera = rotate(view.x, view.y, view.z);

	// The models share one depth buffer, cleared once a frame
	RETRO_ClearDepthBuffer();
	Pose(Dolphin, now / (float)span.length * SWIM_BLENDS);
	Draw(Dolphin, DOLPHIN_HEADING, { 0, 0, 0 }, eye, camera);

	// The fish of a phase share its pose, blended once for all of them
	for (int phase = 0; phase < FISH_PHASES; phase++) {
		Pose(Fish, now / (float)span.length * SWIM_BLENDS * FISH_PACE + phase + 1);
		for (int i = 0; i < FISH; i++) {
			if (i / FISH_PER_PHASE % FISH_PHASES == phase) {
				Draw(Fish, FISH_HEADING, SwimFish(i, now), eye, camera);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *texture = RETRO_LoadImage("assets/dolphin_256x256.pcx");
	RETRO_Palette texturepalette[TEXTURE_COLORS];
	for (int i = 0; i < TEXTURE_COLORS; i++) {
		texturepalette[i] = RETRO_To6bitColor(texture->palette[i]);
	}

	// The shading palette, kept at 8 bits for the fades, which set it
	RETRO_Palette palette[RETRO_COLORS];
	RETRO_CreatePhongShadeTablePalette(texturepalette, TEXTURE_COLORS, palette, SPECULARITY, FALLOFF);
	RETRO_CreatePhongShadeTable(texturepalette, TEXTURE_COLORS, palette, ShadeTable, SPECULARITY, FALLOFF);
	for (int i = 0; i < RETRO_COLORS; i++) {
		Palette[i] = RETRO_To8bitColor(palette[i]);
	}

	Dolphin = RETRO_LoadMD3Model("assets/dolphin.md3", MD3_SCALE);
	Dolphin->texmap = texture->data;
	Dolphin->shadetable = ShadeTable;

	Fish = RETRO_LoadMD3Model("assets/fish2.md3", MD3_SCALE);
	Fish->texmap = texture->data;
	Fish->shadetable = ShadeTable;
}
