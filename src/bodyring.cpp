//
// Body ring
//
// A metal torso turning inside a ring that slides up and down it, with a
// chrome knot tumbling in beside it that shows the torso and ring in its
// surface. The meshes are torso.obj, ring.obj and knot.obj, a closed tube bent
// into a warped loop. Everything is lit by lighting maps, read by the
// view-space normal (RETRO_POLY_MATCAP), 64 texels out from the middle for the
// torso and the ring and 128 for the knot, so the knot sweeps its map's whole
// disk.
//
// The torso's map is a highlight laid out by distance from the middle, with
// the distance across counted at half, so the highlight is twice as wide as
// it is tall. Its bump is not a height field but an offset map,
// bodyring_offsetmap_256x256.pcx, a pair of signed offsets a texel added to
// where the normal reads the map, so the bump stays put in the map as the
// torso turns.
//
// The ring's shadow is sixteen rows of the torso's map moved 32 entries down
// the palette, onto the blue of the shaded metal, at the rows the ring's
// height picks, so the band slides over the torso as the ring does.
//
// The knot's map is drawn afresh every frame: its own picture, with the
// torso and the ring drawn into it, turned the other way and carried half as
// far across.
//
// A keyframe table gives the torso's place, the ring's height on it and the
// knot's place, and each frame mixes the two keyframes it falls between. The
// torso rises in from below, turning to face the viewer as it comes, and the
// ring comes up onto it; the knot flies in from the left as the torso steps
// right; then they pass each other, the torso leaving past the left edge and
// the knot past the right, and the loop starts again. The steady motions are
// not in the table: the torso spins from SPIN_START, the ring bobs from
// BOB_START, the torso rocks and the ring circles it from ROCK_START, and the
// knot tumbles from KNOT_START.
//
// Positions are in eye distances, in the library's axes, x right, y down and
// z away, as the meshes are.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromodel.h"
#include "lib/retromatrix.h"
#include "lib/retrorender.h"

constexpr float FOCAL = 512.0f;            // the eye's distance from the screen, in pixels
constexpr int MAP_SIZE = 256;              // the lighting maps' width and height
constexpr int TORSO_MAP_RADIUS = 64;       // the torso's and the ring's reach from the map's middle
constexpr int KNOT_MAP_RADIUS = 128;       // the knot's reach from the map's middle
constexpr int OFFSET_MAP_WIDTH = 128;      // the offset map's width, two bytes a texel
constexpr int SHADOW_ROWS = 16;            // rows of the torso's map the shadow covers
constexpr int SHADOW_TOP = 136;            // the shadow's first row, less the ring's height
constexpr int SHADOW_REACH = 100;          // the ring's height beyond which it casts no shadow
constexpr int SHADOW_OFFSET = 32;          // the shaded metal is 32 entries past the lit

// Eye distances n pixels span at depth z, for placing things just off screen
constexpr float PIXELS(float n, float z) { return n * (z + 1) / FOCAL; }

constexpr float NECK_HEIGHT = 2.0f;        // eye distances the neck's top is above the torso's middle
constexpr float NECK_DEPTH = 0.25f;        // and at most behind it, as the torso starts turned away
constexpr float TORSO_REACH = 2.2f;        // eye distances the ring reaches across from the torso's axis, circling it
constexpr float TORSO_NEAR = 7.7f;         // the torso's depth as it rises
constexpr float TORSO_FAR = 8.0f;          // and once the knot is in
constexpr float TORSO_BELOW = NECK_HEIGHT + PIXELS(RETRO_HEIGHT / 2.0f, TORSO_NEAR + NECK_DEPTH); // the height it rises from, the neck's top on the bottom edge
constexpr float TORSO_LEAVES = -(TORSO_REACH + PIXELS(RETRO_WIDTH / 2.0f, TORSO_FAR)); // where it leaves to, past the left edge with its ring
constexpr float SPIN_START = 2.3f;         // the time the torso has risen and starts to spin
constexpr float SPIN_SPEED = -74.0f;       // degrees a second it spins
constexpr float ROCK_START = 7.1f;         // the time it starts to rock and the ring to circle it
constexpr float ROCK_ANGLE = 22.5f;        // degrees it rocks either way
constexpr float ROCK_PERIOD = 0.9f;        // seconds a rock takes

constexpr float RING_STEP = 0.053f;        // eye distances the ring moves for a row of its shadow
constexpr float BOB_START = 4.3f;          // the time the ring has landed and starts to bob
constexpr int BOB_ROWS = 32;               // rows it bobs either way, and is from its place as it slows to land
constexpr float BOB_PERIOD = 3.7f;         // seconds a bob takes
constexpr float CIRCLE_RADIUS = 0.43f;     // eye distances it swings off the torso's axis, circling twice a rock

constexpr float KNOT_RADIUS = 2.86f;       // eye distances the knot reaches from its middle, whichever way it turns
constexpr float KNOT_FAR = 20.0f;          // its depth as it flies in
constexpr float KNOT_NEAR = 8.3f;          // and once it is in
constexpr float KNOT_OUTSIDE = -(KNOT_RADIUS + PIXELS(RETRO_WIDTH / 2.0f, KNOT_FAR)); // where it flies in from, past the left edge
constexpr float KNOT_LEAVES = KNOT_RADIUS + PIXELS(RETRO_WIDTH / 2.0f, KNOT_NEAR); // and leaves to, past the right edge
constexpr float KNOT_START = 9.0f;         // the time it flies in at
constexpr vec3 TUMBLE_SPEED = { 6.0f, -43.0f, -18.5f }; // degrees a second it pitches, yaws and rolls

// Where the torso is, the ring's height on it in rows of its shadow, and where
// the knot is, at a time. Between two keyframes everything moves in a
// straight line.
struct Keyframe {
	float time;
	vec3 torso;
	float ring;
	vec3 knot;
};

static const Keyframe Keyframes[] = {
	{ 0.0f, { 0, TORSO_BELOW, TORSO_NEAR }, 200, { KNOT_OUTSIDE, 0, KNOT_FAR } },
	{ 0.9f, { 0, TORSO_BELOW, TORSO_NEAR }, 200, { KNOT_OUTSIDE, 0, KNOT_FAR } },
	{ SPIN_START, { 0, 0, TORSO_NEAR }, 200, { KNOT_OUTSIDE, 0, KNOT_FAR } },
	{ 3.2f, { 0, 0, TORSO_NEAR }, 80, { KNOT_OUTSIDE, 0, KNOT_FAR } },
	{ 3.7f, { 0, 0, TORSO_NEAR }, BOB_ROWS, { KNOT_OUTSIDE, 0, KNOT_FAR } },
	{ BOB_START, { 0, 0, TORSO_NEAR }, 0, { KNOT_OUTSIDE, 0, KNOT_FAR } },
	{ KNOT_START, { 0, 0, TORSO_NEAR }, 0, { KNOT_OUTSIDE, 0, KNOT_FAR } },
	{ 11.0f, { 0, 0, TORSO_NEAR }, 0, { -3.0f, 0, 14.5f } },
	{ 17.6f, { 1.3f, 0, TORSO_FAR }, 0, { -1.5f, 0, KNOT_NEAR } },
	{ 22.9f, { 1.3f, 0, TORSO_FAR }, 0, { -1.5f, 0, KNOT_NEAR } },
	{ 35.0f, { TORSO_LEAVES, 0, TORSO_FAR }, 0, { KNOT_LEAVES, 0, KNOT_NEAR } },
};

constexpr int KEYFRAMES = (int)(sizeof(Keyframes) / sizeof(Keyframes[0]));

// The torso and its ring at a moment: where the torso is, its pitch and yaw,
// and where the ring is on it, turned with it
struct Pose {
	vec3 position;
	float pitch, yaw;
	vec3 ring;
};

static Model3D *Torso;
static Model3D *Ring;
static Model3D *Knot;
static RETRO_Image *TorsoBase;
static RETRO_Image *KnotBase;
static unsigned char TorsoMap[MAP_SIZE * MAP_SIZE];
static unsigned char Reflection[MAP_SIZE * MAP_SIZE];
static unsigned char Scratch[RETRO_WIDTH * RETRO_HEIGHT];

// The model turned, moved by offset in its own frame and then to position,
// and projected around cx, cy
static void Place(Model3D *model, const mat3 &matrix, vec3 offset, vec3 position, float cx, float cy)
{
	RETRO_RotateModel(matrix, model);
	vec3 move = matrix * offset + position;
	RETRO_TranslateModel(move.x, move.y, move.z, model);
	RETRO_ProjectModel(FOCAL, cx, cy, model, FOCAL);
}

// The torso and its ring, the screen's middle at cx, cy
static void DrawTorso(const Pose &pose, float cx, float cy, bool cleardepth, ClipRect clip = {})
{
	mat3 matrix = rotate(pose.pitch, pose.yaw, 0);
	Place(Torso, matrix, { 0, 0, 0 }, pose.position, cx, cy);
	Place(Ring, matrix, pose.ring, pose.position, cx, cy);
	RETRO_RenderModel(RETRO_POLY_MATCAP, RETRO_SHADE_NONE, Torso, cleardepth, clip);
	RETRO_RenderModel(RETRO_POLY_MATCAP, RETRO_SHADE_NONE, Ring, false, clip);
}

// The map's rows from top to bottom of the reflection, centred in the map.
// The drawers write to the framebuffer, so it is pointed at a scratch screen
// for the while, the rows laid in its left columns from its row first
static void DrawReflectionRows(const Pose &pose, int top, int first, int bottom)
{
	int shift = top - first;
	for (int y = top; y < bottom; y++) {
		memcpy(&Scratch[(y - shift) * RETRO_WIDTH], &Reflection[y * MAP_SIZE], MAP_SIZE);
	}

	unsigned char *framebuffer = RETRO.framebuffer;
	RETRO.framebuffer = Scratch;
	Pose reflected = pose;
	reflected.position.x /= 2;
	reflected.yaw = (float)M_PI - pose.yaw;
	DrawTorso(reflected, MAP_SIZE / 2.0f, MAP_SIZE / 2.0f - shift, true, { 0, MAP_SIZE, first, bottom - shift });
	RETRO.framebuffer = framebuffer;

	for (int y = top; y < bottom; y++) {
		memcpy(&Reflection[y * MAP_SIZE], &Scratch[(y - shift) * RETRO_WIDTH], MAP_SIZE);
	}
}

// The knot's own picture with the torso and the ring drawn into it, turned
// the other way and carried half as far across. The screen is shorter than
// the map, so the rows past its bottom are drawn again with the map moved up
// by the difference, clipped to them
static void DrawReflection(const Pose &pose)
{
	memcpy(Reflection, KnotBase->data, sizeof(Reflection));
	DrawReflectionRows(pose, 0, 0, RETRO_HEIGHT);
	DrawReflectionRows(pose, RETRO_HEIGHT, 2 * RETRO_HEIGHT - MAP_SIZE, MAP_SIZE);
}

void DEMO_Render(RETRO_Time time)
{
	// The two keyframes this moment falls between. The time leads each one,
	// so the keyframes are walked a keyframe apart
	RETRO_KeyframeLoop span = RETRO_LoopKeyframes(time.total, Keyframes, KEYFRAMES, sizeof(Keyframe));
	const Keyframe &from = Keyframes[span.key];
	const Keyframe &to = Keyframes[span.key + 1];
	double seconds = span.seconds;
	float t = span.t;

	// The ring's height from the keyframes, and its bob on top, carrying on up
	// past its place as it lands and then settling back. Its shadow moves a
	// row for each row of height, in whole rows
	float bob = 0;
	if (seconds >= BOB_START) {
		bob = -BOB_ROWS * (float)sin(2 * M_PI * (seconds - BOB_START) / BOB_PERIOD);
	}
	float ringheight = mix(from.ring, to.ring, t) + bob;
	int shadowrow = (int)floor(ringheight);

	// The torso turns to face the viewer as it rises, half a turn over the
	// rise, then spins and rocks, and the ring circles it
	Pose pose = {};
	pose.position = mix(from.torso, to.torso, t);
	pose.yaw = (float)radians(180 * pose.position.y / TORSO_BELOW + SPIN_SPEED * MAX(seconds - SPIN_START, 0.0));
	pose.ring.y = ringheight * RING_STEP;
	if (seconds >= ROCK_START) {
		double phase = 2 * M_PI * (seconds - ROCK_START) / ROCK_PERIOD;
		pose.pitch = (float)radians(ROCK_ANGLE * sin(phase));
		pose.ring.x = CIRCLE_RADIUS * (float)sin(2 * phase);
		pose.ring.z = -CIRCLE_RADIUS * (float)cos(2 * phase);
	}

	// The ring's shadow, once it is near enough the torso to cast one
	memcpy(TorsoMap, TorsoBase->data, sizeof(TorsoMap));
	if (shadowrow > -SHADOW_REACH && shadowrow < SHADOW_REACH) {
		int row = (shadowrow + SHADOW_TOP) * MAP_SIZE;
		for (int i = row; i < row + SHADOW_ROWS * MAP_SIZE; i++) {
			TorsoMap[i] += SHADOW_OFFSET;
		}
	}

	bool cleardepth = true;
	if (seconds >= KNOT_START) {
		DrawReflection(pose);
		vec3 tumble = TUMBLE_SPEED * (float)(seconds - KNOT_START);
		mat3 matrix = rotate((float)radians(tumble.x), (float)radians(tumble.y), (float)radians(tumble.z));
		Place(Knot, matrix, { 0, 0, 0 }, mix(from.knot, to.knot, t), RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f);
		RETRO_RenderModel(RETRO_POLY_MATCAP, RETRO_SHADE_NONE, Knot);
		cleardepth = false;
	}
	DrawTorso(pose, RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f, cleardepth);
}

void DEMO_Initialize(void)
{
	TorsoBase = RETRO_LoadImage("assets/bodyring_torsomap_256x256.pcx", true);
	KnotBase = RETRO_LoadImage("assets/bodyring_knotmap_256x256.pcx");
	RETRO_Image *offsets = RETRO_LoadImage("assets/bodyring_offsetmap_256x256.pcx");

	Torso = RETRO_Load3DModel("assets/torso.obj");
	Torso->envmap = TorsoMap;
	Torso->envmapradius = TORSO_MAP_RADIUS;
	Torso->offsetmap = (signed char *)offsets->data;
	Torso->offsetmapwidth = OFFSET_MAP_WIDTH;
	Torso->offsetmapheight = offsets->height;

	Ring = RETRO_Load3DModel("assets/ring.obj");
	Ring->envmap = RETRO_LoadImage("assets/bodyring_ringmap_256x256.pcx")->data;
	Ring->envmapradius = TORSO_MAP_RADIUS;

	Knot = RETRO_Load3DModel("assets/knot.obj");
	Knot->envmap = Reflection;
	Knot->envmapradius = KNOT_MAP_RADIUS;
}
