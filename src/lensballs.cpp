//
// Lens balls
//
// Ten glass balls bouncing over a hidden picture, which shows only through
// them.
//
// A ball is an 80x80 sprite with three tables behind it, all from the height
// of a sphere of radius R = 40 over each pixel, z = sqrt(R^2 - r^2) at r from
// the middle, and 0 outside the ball.
//
// The first is a texture offset per pixel into the 256x256 picture, a
// stereographic projection of the sphere:
//
//   u = R + dx * R / (R + z),  v = R + dy * R / (R + z)
//
// which magnifies the middle twice and squeezes the picture together toward
// the rim. The ball's screen position is added to it, so the picture stays put
// and each ball is a lens moving over it.
//
// The second is the height z itself, kept per screen pixel as depth, so
// overlapping balls cut into each other along the curves where the spheres
// meet instead of one covering the other.
//
// The third is a 160x160 light map, a cone of SHADE_LEVELS levels around its
// middle, of which each ball reads an 80x80 window. The window is the ball's
// position on the screen scaled to the map's 80 pixels of room, so the
// highlight always faces the middle of the screen, as if lit by a lamp there,
// and slides across the ball as it moves. The cone stops falling at
// LIGHT_FLOOR, so a ball far from the middle stays dim rather than going
// flat gray.
//
// The light level picks a row of a shade table built from the picture's
// palette, so any 256x256 picture works. Level 16 leaves a color as it is.
// Below it the color blends toward a dark gray, reached at level 0, and above
// it toward white, matched back to the nearest palette color.
//
// The balls bounce off a box that keeps them MARGIN pixels inside the screen,
// and one more joins every BALL_INTERVAL seconds until all ten are moving.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retroshadetable.h"

constexpr int BALLS = 10;
constexpr int BALL_SIZE = 80;
constexpr int PICTURE_SIZE = 256;
constexpr int BALL_RADIUS = BALL_SIZE / 2;
constexpr float BALL_INTERVAL = 0.73f;        // seconds between balls joining
constexpr int LIGHT_WIDTH = 160;
constexpr float LIGHT_SLOPE = 0.6f;           // levels lost a pixel from the highlight
constexpr int LIGHT_FLOOR = 10;               // the level the light falls no lower than
constexpr int SHADE_LEVELS = 32;
constexpr int UNLIT_LEVEL = SHADE_LEVELS / 2; // the level that leaves a color as it is

// The bounce box, in pixels
constexpr int MARGIN = 10;
constexpr int ROOM_X = RETRO_WIDTH - BALL_SIZE - 2 * MARGIN;    // how far a ball can move each way
constexpr int ROOM_Y = RETRO_HEIGHT - BALL_SIZE - 2 * MARGIN;

struct Ball {
	float x, y, dx, dy;
};

// Where each ball starts, in pixels, and how fast it moves, in pixels a second
static const Ball Balls[BALLS] = {
	{ 160, 100, 17.5f, -35 }, { 160, 100, -17.5f, 35 }, { 160, 100, 35, -70 }, { 160, 100, 17.5f, 87.5f },
	{ 160, 100, -87.5f, -87.5f }, { 160, 100, 35, 35 }, { 160, 100, -105, 70 }, { 160, 100, -17.5f, 70 },
	{ 120, 100, 70, -35 }, { 180, 80, 35, -70 },
};

static const unsigned char *Picture;
static unsigned char Shades[RETRO_COLORS * SHADE_LEVELS];
static float Height[BALL_SIZE * BALL_SIZE];
static unsigned char OffsetU[BALL_SIZE * BALL_SIZE];
static unsigned char OffsetV[BALL_SIZE * BALL_SIZE];
static unsigned char Light[LIGHT_WIDTH * LIGHT_WIDTH];

// A color at a light level, given as a fraction of the top one
static RETRO_Palette Shade(RETRO_Palette color, float shade, const float *)
{
	float level = shade * (SHADE_LEVELS - 1);
	return level < UNLIT_LEVEL
		? mix(RETRO_GRAPHITE, color, level / UNLIT_LEVEL)
		: mix(color, RETRO_WHITE, (level - UNLIT_LEVEL) / UNLIT_LEVEL);
}

//
// The sphere's height and texture offsets, see the top of the file
//
static void CreateBall(void)
{
	for (int y = 0; y < BALL_SIZE; y++) {
		for (int x = 0; x < BALL_SIZE; x++) {
			int dx = x - BALL_RADIUS;
			int dy = y - BALL_RADIUS;
			int r2 = dx * dx + dy * dy;
			if (r2 >= BALL_RADIUS * BALL_RADIUS) continue;

			int i = y * BALL_SIZE + x;
			float z = sqrt(BALL_RADIUS * BALL_RADIUS - r2);
			int u = BALL_RADIUS + (int)floorf(dx * BALL_RADIUS / (BALL_RADIUS + z));
			int v = BALL_RADIUS + (int)floorf(dy * BALL_RADIUS / (BALL_RADIUS + z));
			Height[i] = z;
			OffsetU[i] = u;
			OffsetV[i] = v;
		}
	}
}

//
// Where a ball is along one axis after time seconds, moving at a steady speed
// and bouncing within room pixels from MARGIN: a triangle wave of the distance
// traveled
//
static float Bounce(float start, float speed, float room, float time)
{
	float p = mod(start - MARGIN + speed * time, 2 * room);
	return MARGIN + (p <= room ? p : 2 * room - p);
}

void DEMO_Render(RETRO_Time time)
{
	unsigned char *screen = RETRO_FrameBuffer();
	RETRO_ClearDepthBuffer();

	int visible = MIN((int)(time.total / BALL_INTERVAL) + 1, BALLS);
	for (int i = 0; i < visible; i++) {
		// Each ball moves from the moment it joins
		float moving = time.total - i * BALL_INTERVAL;
		int sx = (int)Bounce(Balls[i].x, Balls[i].dx, ROOM_X, moving);
		int sy = (int)Bounce(Balls[i].y, Balls[i].dy, ROOM_Y, moving);

		// The ball's window into the light map, its position scaled to the
		// map's room
		int lightx = (sx - MARGIN) * (LIGHT_WIDTH - BALL_SIZE) / ROOM_X;
		int lighty = (sy - MARGIN) * (LIGHT_WIDTH - BALL_SIZE) / ROOM_Y;
		const unsigned char *light = Light + lighty * LIGHT_WIDTH + lightx;

		for (int y = 0; y < BALL_SIZE; y++) {
			for (int x = 0; x < BALL_SIZE; x++) {
				int local = y * BALL_SIZE + x;
				int dest = (sy + y) * RETRO_WIDTH + sx + x;
				if (!RETRO_DepthTest(dest, Height[local])) continue;

				// The picture stays put: the ball's position moves its offsets, u and
				// v each wrapping on its own so the picture tiles
				int u = (OffsetU[local] + sx) % PICTURE_SIZE;
				int v = (OffsetV[local] + sy) % PICTURE_SIZE;
				unsigned char color = Picture[v * PICTURE_SIZE + u];
				screen[dest] = Shades[color * SHADE_LEVELS + light[y * LIGHT_WIDTH + x]];
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *picture = RETRO_LoadImage("assets/einstein_256x256.pcx", true);
	Picture = picture->data;
	RETRO_CreateShadeTable(picture->palette, picture->palette, { Shades, RETRO_COLORS, SHADE_LEVELS }, Shade);
	RETRO_CreateRoundAnglePhongMap(Light, LIGHT_WIDTH, LIGHT_WIDTH, SHADE_LEVELS - 1, LIGHT_SLOPE * (LIGHT_WIDTH - 1) / 2, LIGHT_FLOOR);
	CreateBall();
}
