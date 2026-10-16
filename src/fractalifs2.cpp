//
// Morphing iterated function system
//
// A system is twelve affine maps of the screen plane, in pixels, each a matrix
// and an offset,
//
//   w_i(p) = M_i p + offset_i
//
// and the attractor is drawn by the chaos game: one point is carried by a map
// drawn at random every step and plotted where it lands. Each map also carries
// a weight, and the weights are dealt out in proportion as runs of a 256 entry
// table, so one random byte picks a map with the probability its weight gives
// it.
//
// Eight systems are authored, and the picture morphs from one to the next by
// mixing every coefficient of a map with the one in the same slot of the next
// system, weights included. The mix follows a half cosine over SYSTEM_TIME
// seconds, so a system eases out of the one before and into the one after. The
// last system morphs back into the first, so the sequence loops. A system with
// fewer than twelve maps repeats some of them, which is what lets it morph slot
// by slot into one with more.
//
// The random bytes are dealt once, and every frame replays the same ones, so a
// system that holds still is drawn the same each frame and the orbit does not
// shimmer. The points are drawn into the two low bits of the background, once
// in place and once SHADOW pixels down and to the right as its shadow.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_WIDTH 640
#define RETRO_HEIGHT 480

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromatrix.h"

constexpr int SYSTEMS = 8;
constexpr int SLOTS = 12; // maps a system holds, repeats included
constexpr double SYSTEM_TIME = 1.8; // seconds a morph to the next system takes
constexpr int CHOICES = 256; // entries of the weighted map table
constexpr int POINTS = 65535; // points the chaos game plots a frame
constexpr int TRANSIENT = 30; // points dropped while the orbit falls onto the attractor
constexpr int SHADOW = 16; // pixels the shadow lies down and to the right

constexpr unsigned char COLOR_POINT = 1;
constexpr unsigned char COLOR_SHADOW = 2;

struct Map {
	mat2 m; // w(p) = m p + offset, its columns where the x and y axes land
	vec2 offset;
	int weight; // its share of the draws
	int slots; // slots it fills in its system
};

// The authored systems, each map listed once with the slots it fills. The
// Barnsley fern and the Sierpinski triangle are the classic codes; the rest are
// named for what they draw
static const Map Systems[SYSTEMS][SLOTS] = {
	{ // Single point
	  { { { 0.000f, 0.000f }, { 0.000f, 0.000f } }, { 312.500f, 225.000f }, 22, 12 } },
	{ // Spiral of spirals
	  { { { 0.965f, -0.209f }, { 0.333f, 0.800f } }, { -76.625f, 140.625f }, 27, 9 },
	  { { { 0.145f, 0.072f }, { -0.133f, 0.200f } }, { 225.000f, -6.250f }, 6, 3 } },
	{ // Barnsley fern, on its side
	  { { { 0.862f, 0.040f }, { -0.040f, 0.850f } }, { 98.750f, 37.500f }, 73, 3 },
	  { { { 0.220f, -0.260f }, { 0.230f, 0.200f } }, { 56.875f, 187.500f }, 6, 3 },
	  { { { 0.240f, 0.280f }, { 0.260f, -0.150f } }, { -2.500f, 287.500f }, 6, 3 },
	  { { { 0.160f, 0.000f }, { 0.000f, 0.000f } }, { 0.000f, 243.750f }, 1, 3 } },
	{ // Koch curve, stretched
	  { { { 0.338f, 0.000f }, { 0.033f, 0.433f } }, { 395.250f, 166.875f }, 22, 3 },
	  { { { 0.164f, 0.287f }, { -0.363f, 0.218f } }, { 419.625f, 54.375f }, 21, 3 },
	  { { { 0.164f, -0.287f }, { 0.363f, 0.218f } }, { 111.375f, 238.125f }, 21, 3 },
	  { { { 0.331f, 0.000f }, { -0.021f, 0.433f } }, { 26.375f, 166.875f }, 22, 3 } },
	{ // Curled dragon
	  { { { 0.500f, 0.000f }, { 0.000f, 0.500f } }, { 223.375f, 55.750f }, 15, 6 },
	  { { { -0.062f, -0.438f }, { 0.500f, -0.250f } }, { 119.250f, 348.750f }, 28, 3 },
	  { { { 0.438f, 0.062f }, { 0.250f, -0.500f } }, { 208.375f, 444.125f }, 28, 3 } },
	{ // Sierpinski triangle
	  { { { 0.500f, 0.000f }, { 0.000f, 0.500f } }, { 7.500f, 12.500f }, 15, 6 },
	  { { { 0.500f, 0.000f }, { 0.000f, 0.500f } }, { 312.500f, 12.500f }, 28, 3 },
	  { { { 0.500f, 0.000f }, { 0.000f, 0.500f } }, { 160.000f, 237.500f }, 28, 3 } },
	{ // Star tiles
	  { { { 0.697f, -0.394f }, { -0.481f, -0.663f } }, { 197.750f, 601.000f }, 31, 6 },
	  { { { 0.091f, 0.515f }, { -0.443f, -0.095f } }, { 457.875f, 5.000f }, 12, 6 } },
	{ // Feathered spiral
	  { { { 0.088f, -0.464f }, { 0.521f, -0.378f } }, { 325.750f, 537.500f }, 9, 6 },
	  { { { 0.824f, -0.212f }, { 0.281f, 0.864f } }, { -35.750f, 78.125f }, 34, 6 } },
};

static Map Slots[SYSTEMS][SLOTS]; // the systems with their repeats laid out slot by slot
static unsigned char Random[TRANSIENT + POINTS];

void DEMO_Render(RETRO_Time time)
{
	// Mix every map of this system with the same slot of the next, easing from
	// all of this one to all of the next over the half cosine
	double phase = fmod(time.total / SYSTEM_TIME, SYSTEMS);
	int system = (int)phase;
	float ease = (1 - cos(M_PI * (phase - system))) / 2;

	Map maps[SLOTS];
	int total = 0;
	for (int i = 0; i < SLOTS; i++) {
		const Map *from = &Slots[system][i];
		const Map *to = &Slots[(system + 1) % SYSTEMS][i];
		maps[i].m = { mix(from->m.col0, to->m.col0, ease), mix(from->m.col1, to->m.col1, ease) };
		maps[i].offset = mix(from->offset, to->offset, ease);
		maps[i].weight = (int)mix(from->weight, to->weight, ease);
		total += maps[i].weight;
	}

	// Deal each map its share of the table, ending its run where its share of
	// the running total falls, so the runs fill the table exactly
	unsigned char choices[CHOICES];
	int cursor = 0;
	int sum = 0;
	for (int i = 0; i < SLOTS; i++) {
		sum += maps[i].weight;
		for (; cursor < CHOICES * sum / total; cursor++) {
			choices[cursor] = i;
		}
	}

	RETRO_Blit(RETRO_ImageData());
	unsigned char *buffer = RETRO_FrameBuffer();

	// Carry the point by the map each random byte picks, plotting it once the
	// transient is past
	vec2 p = { 0, 0 };
	for (int i = 0; i < TRANSIENT + POINTS; i++) {
		const Map *w = &maps[choices[Random[i]]];
		p = w->m * p + w->offset;
		if (i < TRANSIENT) {
			continue;
		}

		if (RETRO_OnScreen(p.x, p.y)) {
			buffer[(int)p.y * RETRO_WIDTH + (int)p.x] |= COLOR_POINT;
		}
		if (RETRO_OnScreen(p.x + SHADOW, p.y + SHADOW)) {
			buffer[(int)(p.y + SHADOW) * RETRO_WIDTH + (int)(p.x + SHADOW)] |= COLOR_SHADOW;
		}
	}
}

void DEMO_Initialize(void)
{
	// Background and palette. Its pixels keep the two low bits clear for the
	// points and the shadow, and the palette colors every mix of the two
	RETRO_LoadImage("assets/squiggles_640x480.pcx", true);

	// Lay every system out over all its slots, each map repeated for the slots
	// it fills, so any two systems morph slot by slot
	for (int s = 0; s < SYSTEMS; s++) {
		int slot = 0;
		for (int m = 0; slot < SLOTS; m++) {
			for (int i = 0; i < Systems[s][m].slots; i++) {
				Slots[s][slot++] = Systems[s][m];
			}
		}
	}

	// The random bytes every frame replays, one per step of the chaos game
	for (int i = 0; i < TRANSIENT + POINTS; i++) {
		Random[i] = RANDOM(256);
	}
}
