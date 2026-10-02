//
// Snow
//
// Snow falling through a night sky in layers of depth, blown by a gusting
// wind, and settling on a logo and the ground in drifts that slowly melt.
//
// A flake has a depth z in [0, 1], far to near, and everything about it
// follows from that: it falls at
//
//   vy = SNOW_MINSPEED + z (SNOW_MAXSPEED − SNOW_MINSPEED)
//
// grows from a dim single pixel to a bright plus, and is carried by the
// wind in proportion to 0.3 + z, so the near layers stream past the far
// ones. Each flake also sways by a sine of its own rate and phase. The wind
// is a sum of two slow sines, so it gusts and drops and now and then turns.
//
// Flakes nearer than SNOW_SETTLE land. The world is a grid of cells, empty,
// ground, logo or snow, and a flake that would move into a filled cell
// stops there and slides, sand-style: down if it can, else down one cell
// across, else down two across, each toward the wind first, and it becomes
// a snow cell where none of those is free. Snow therefore heaps at slopes
// up to one in two, runs off the letters' shoulders and fills the dips. The
// world wraps round the sides, as the flakes do.
//
// SNOW_MELT random columns a second are tested for melting. Every layer of snow
// in the column with open air above it, on the logo or on the ground beneath
// it, melts from the top by d / SNOW_DEPTH cells on average, d being the
// layer's depth, so a thin cap lasts and a deep drift wastes away. Every
// surface fills to about the same depth, where melting keeps up with the
// snowfall, and holds it.
//
// Far flakes pass behind the logo and leave the bottom of the screen, and
// every flake that leaves comes back at a random place above the top.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"

#define FLAKES 900
#define SNOW_MINSPEED 12.0f // pixels a second, farthest layer
#define SNOW_MAXSPEED 46.0f // nearest layer
#define SNOW_SWAY 10.0 // pixels a second of each flake's own sway
#define SNOW_WIND 28.0 // pixels a second, the gusts' strength
#define SNOW_SETTLE 0.6 // flakes nearer than this land
#define SNOW_MELT 50.0 // columns a second whose top is tested for melting
#define SNOW_DEPTH 16.0 // depth of snow that melts one cell, on average, per test
#define GROUND_HEIGHT 14 // pixels of ground at its lowest
#define LOGO_TEXT "RETRO"
#define LOGO_SCALE 6 // pixels per font pixel
#define LOGO_Y 90 // top of the logo
#define SKY 0 // palette ranges
#define SKY_COLORS 64
#define FLAKE SKY_COLORS
#define FLAKE_COLORS 16
#define GROUND (FLAKE + FLAKE_COLORS)
#define LOGO (GROUND + 1)
#define SNOW (LOGO + 1)

enum { EMPTY, SOLID, SETTLED };

struct Flake {
	float x;
	float y;
	float z;
	float swayphase;
	float swayspeed;
};

static Flake Flakes[FLAKES];
static unsigned char World[RETRO_WIDTH * RETRO_HEIGHT]; // EMPTY, SOLID or SETTLED
static unsigned char Scenery[RETRO_WIDTH * RETRO_HEIGHT]; // palette entries of the ground and logo, 0 elsewhere

//
// Whether a cell is filled, wrapping round the sides as the flakes do, with
// the bottom counting as filled and everything above the top as empty
//
static bool Filled(int x, int y)
{
	if (y >= RETRO_HEIGHT) {
		return true;
	}
	return y >= 0 && World[y * RETRO_WIDTH + WRAP(x, RETRO_WIDTH)] != EMPTY;
}

//
// Start a flake at a random place, above the top of the screen when fresh
//
static void Respawn(Flake *flake, bool fresh)
{
	flake->x = RANDOMF(RETRO_WIDTH);
	flake->y = fresh ? -RANDOMF(20) - 2 : RANDOMF(RETRO_HEIGHT);
	flake->z = RANDOMF(1);
	flake->swayphase = RANDOMF(2 * M_PI);
	flake->swayspeed = mix(0.6, 1.8, RAND());
}

//
// Settle a flake into the world at (x, y) or, sand-style, a cell below
//
static void Settle(int x, int y, int downwind)
{
	// Straight down, then one across and down, then two, downwind first
	const int slides[5] = { 0, downwind, -downwind, 2 * downwind, -2 * downwind };

	while (y < RETRO_HEIGHT - 1) {
		bool moved = false;
		for (int slide : slides) {
			bool clear = slide * slide < 4 || !Filled(x + slide / 2, y);
			if (clear && !Filled(x + slide, y + 1)) {
				x += slide;
				y++;
				moved = true;
				break;
			}
		}
		if (!moved) {
			break;
		}
	}
	if (y >= 0 && !Filled(x, y)) {
		World[y * RETRO_WIDTH + WRAP(x, RETRO_WIDTH)] = SETTLED;
	}
}

void DEMO_FixedUpdate(double timestep)
{
	// Calculate wind
	static double time = 0;
	time += timestep;
	float wind = SNOW_WIND * (0.7f * sin(fmod(0.13 * time, 2 * M_PI)) + 0.5f * sin(fmod(0.37 * time, 2 * M_PI)) + 0.2f);
	int downwind = wind >= 0 ? 1 : -1;

	// Move the flakes
	for (Flake &flake : Flakes) {
		int oldx = (int)floorf(flake.x);
		int oldy = (int)floorf(flake.y);

		flake.swayphase = fmodf(flake.swayphase + flake.swayspeed * timestep, 2 * M_PI);
		flake.x += (wind * (0.3f + flake.z) + SNOW_SWAY * sinf(flake.swayphase)) * timestep;
		flake.y += mix(SNOW_MINSPEED, SNOW_MAXSPEED, flake.z) * timestep;

		if (flake.z >= SNOW_SETTLE) {
			int x = (int)floorf(flake.x);
			int y = (int)floorf(flake.y);
			if ((x != oldx || y != oldy) && Filled(x, y)) {
				Settle(oldx, oldy, downwind);
				Respawn(&flake, true);
				continue;
			}
		}

		flake.x -= RETRO_WIDTH * floorf(flake.x / RETRO_WIDTH);
		if (flake.y >= RETRO_HEIGHT) {
			Respawn(&flake, true);
		}
	}

	// Melt every exposed layer of snow in a few random columns, the deeper the more
	static double melt = 0;
	for (melt += SNOW_MELT * timestep; melt >= 1; melt--) {
		int x = RANDOM(RETRO_WIDTH);
		for (int top = 0; top < RETRO_HEIGHT; top++) {
			if (World[top * RETRO_WIDTH + x] != SETTLED || (top > 0 && World[(top - 1) * RETRO_WIDTH + x] != EMPTY)) {
				continue;
			}
			int depth = 0;
			while (top + depth < RETRO_HEIGHT && World[(top + depth) * RETRO_WIDTH + x] == SETTLED) {
				depth++;
			}
			int melted = MIN((int)(depth / SNOW_DEPTH + RAND()), depth);
			for (int y = top; y < top + melted; y++) {
				World[y * RETRO_WIDTH + x] = EMPTY;
			}
			top += depth;
		}
	}
}

//
// Draw a flake by its depth: a dim pixel far away, a bright plus up close
//
static void DrawFlake(unsigned char *buffer, const Flake &flake)
{
	int x = (int)floorf(flake.x);
	int y = (int)floorf(flake.y);
	unsigned char color = FLAKE + MIN((int)(flake.z * FLAKE_COLORS), FLAKE_COLORS - 1);
	int size = flake.z < 0.4f ? 0 : flake.z < 0.8f ? 1 : 2;
	static const int plus[5][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };
	int points = size == 0 ? 1 : size == 1 ? 3 : 5;

	for (int i = 0; i < points; i++) {
		int px = x + plus[i][0];
		int py = y + plus[i][1];
		if (px >= 0 && px < RETRO_WIDTH && py >= 0 && py < RETRO_HEIGHT) {
			buffer[py * RETRO_WIDTH + px] = color;
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Draw sky
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		memset(buffer + y * RETRO_WIDTH, SKY + y * SKY_COLORS / RETRO_HEIGHT, RETRO_WIDTH);
	}

	// Draw the far flakes, then the scenery and snow over them, then the near flakes
	for (Flake &flake : Flakes) {
		if (flake.z < SNOW_SETTLE) {
			DrawFlake(buffer, flake);
		}
	}
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		if (World[i] == SETTLED) {
			buffer[i] = SNOW;
		} else if (Scenery[i]) {
			buffer[i] = Scenery[i];
		}
	}
	for (Flake &flake : Flakes) {
		if (flake.z >= SNOW_SETTLE) {
			DrawFlake(buffer, flake);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The sky from night blue down to a dim horizon glow, the
	// flakes from gray-blue far away to white up close
	RETRO_CreateGradientPalette(SKY, SKY + SKY_COLORS, { 4, 6, 24 }, { 40, 44, 78 });
	RETRO_CreateGradientPalette(FLAKE, FLAKE + FLAKE_COLORS, { 90, 100, 130 }, { 255, 255, 255 });
	RETRO_SetColor(GROUND, { 22, 20, 30 });
	RETRO_SetColor(LOGO, { 170, 24, 40 });
	RETRO_SetColor(SNOW, { 225, 232, 250 });

	// Init ground, gently rolling
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int height = GROUND_HEIGHT + (int)(6 * (1 + sin(2 * M_PI * x / RETRO_WIDTH * 2 + 0.5)));
		for (int y = RETRO_HEIGHT - height; y < RETRO_HEIGHT; y++) {
			World[y * RETRO_WIDTH + x] = SOLID;
			Scenery[y * RETRO_WIDTH + x] = GROUND;
		}
	}

	// Init logo, the font scaled up
	int length = (int)strlen(LOGO_TEXT);
	int left = (RETRO_WIDTH - length * 8 * LOGO_SCALE) / 2;
	for (int i = 0; i < length; i++) {
		const unsigned char *glyph = RETRO_Glyph(LOGO_TEXT[i]);
		for (int y = 0; y < 8 * LOGO_SCALE; y++) {
			for (int x = 0; x < 8 * LOGO_SCALE; x++) {
				if (RETRO_GlyphPixel(glyph, x / LOGO_SCALE, y / LOGO_SCALE)) {
					int j = (LOGO_Y + y) * RETRO_WIDTH + left + i * 8 * LOGO_SCALE + x;
					World[j] = SOLID;
					Scenery[j] = LOGO;
				}
			}
		}
	}

	// Init flakes, spread over the screen so the snow has already started
	for (Flake &flake : Flakes) {
		Respawn(&flake, false);
	}
}
