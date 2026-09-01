//
// Iterated function system
//
// Four classic attractors drawn by the chaos game. A system is a handful of
// affine maps of the plane,
//
//   w_i(x, y) = (a x + b y + e,  c x + d y + f)
//
// each a contraction. One point is carried by a map drawn at random every step,
//
//   p' = w_i(p),   i with probability P_i
//
// and the orbit falls onto the attractor: the one set the union of the maps
// leaves unchanged. It is drawn without ever being solved for, and without a
// stack, because the orbit visits the whole of it and nothing else. What the
// probabilities buy is only how the points are spread over it - a map is worth
// roughly its |det|, the share of the area it lands on, and the fern's stem map
// gets a hundredth of the draws because it is a line segment. The first
// TRANSIENT points are dropped: the orbit starts off the attractor and lands on
// it geometrically fast, but the first few are visibly off it.
//
// The picture is a density: a pixel counts the points that land on it and is
// shaded through a log ramp, so a point that lands ten times over is not ten
// times the brightness. Nothing is cleared between frames, so a system is dealt
// out over TIME_DRAW seconds and thickens as it goes.
//
// Tab deals the next system: the Barnsley fern, the Sierpinski triangle, the
// Heighway dragon and a binary tree, each with the window that frames it and
// its own ramp.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define MAX_TRANSFORMS 4
#define POINTS_PER_SECOND 240000 // points the chaos game draws a second
#define TRANSIENT 20 // points dropped while the orbit falls onto the attractor
#define DENSITY_MAX 255 // points a pixel counts before it stops counting
#define TIME_DRAW 6.0 // seconds a system is dealt for
#define TIME_HOLD 2.0 // seconds the finished system is held
#define MARGIN 0.92 // of the screen the framed attractor fills

struct Transform {
	float a, b, c, d, e, f; // w(x, y) = (a x + b y + e, c x + d y + f)
	float p; // its share of the draws
};

struct System {
	Transform transform[MAX_TRANSFORMS];
	int transforms;
	float left, right, bottom, top; // the window on the plane that frames it
	RETRO_Palette low, high; // the ramp a pixel's density is shaded through
};

System Systems[] = {
	{ // Barnsley fern
		{ { 0.00f,  0.00f,  0.00f, 0.16f, 0.00f, 0.00f, 0.01f },
		  { 0.85f,  0.04f, -0.04f, 0.85f, 0.00f, 1.60f, 0.85f },
		  { 0.20f, -0.26f,  0.23f, 0.22f, 0.00f, 1.60f, 0.07f },
		  { -0.15f, 0.28f,  0.26f, 0.24f, 0.00f, 0.44f, 0.07f } }, 4,
		-2.20f, 2.70f, 0.00f, 10.00f, RETRO_HUNTERGREEN, RETRO_SPRINGGREEN },
	{ // Sierpinski triangle
		{ { 0.50f, 0.00f, 0.00f, 0.50f, 0.000f, 0.000f, 0.334f },
		  { 0.50f, 0.00f, 0.00f, 0.50f, 0.500f, 0.000f, 0.333f },
		  { 0.50f, 0.00f, 0.00f, 0.50f, 0.250f, 0.433f, 0.333f } }, 3,
		0.00f, 1.00f, 0.00f, 0.87f, RETRO_DARKRED, RETRO_GOLD },
	{ // Heighway dragon
		{ {  0.50f, -0.50f, 0.50f,  0.50f, 0.00f, 0.00f, 0.50f },
		  { -0.50f, -0.50f, 0.50f, -0.50f, 1.00f, 0.00f, 0.50f } }, 2,
		-0.34f, 1.17f, -0.34f, 0.67f, RETRO_DARKMIDNIGHTBLUE, RETRO_PALESKYBLUE },
	{ // Binary tree
		{ { 0.00f,  0.00f, 0.00f, 0.50f, 0.00f, 0.00f, 0.05f },
		  { 0.42f, -0.42f, 0.42f, 0.42f, 0.00f, 0.20f, 0.40f },
		  { 0.42f,  0.42f, -0.42f, 0.42f, 0.00f, 0.20f, 0.40f },
		  { 0.10f,  0.00f, 0.00f, 0.10f, 0.00f, 0.20f, 0.15f } }, 4,
		-0.24f, 0.24f, 0.00f, 0.44f, RETRO_SADDLEBROWN, RETRO_JASMINE },
};

#define SYSTEMS ((int)(sizeof(Systems) / sizeof(Systems[0])))

unsigned char Density[RETRO_WIDTH * RETRO_HEIGHT];
unsigned char Shade[DENSITY_MAX + 1];
float Cumulative[MAX_TRANSFORMS]; // the draw's cut points, so one RAND picks a map
float PointX, PointY;
float ScreenScale, ScreenX, ScreenY;
int Current = 0;

//
// Carry the point by one map, drawn at random
//
// The cut points are cumulative, so the last one is the whole of the draw and
// the walk cannot run off the end of the table
//
void StepPoint(System *system)
{
	float draw = RAND() * Cumulative[system->transforms - 1];
	int t = 0;
	while (draw > Cumulative[t]) {
		t++;
	}

	Transform *w = &system->transform[t];
	float x = w->a * PointX + w->b * PointY + w->e;
	PointY = w->c * PointX + w->d * PointY + w->f;
	PointX = x;
}

//
// Frame a system, clear what the last one left, and drop the transient
//
void StartSystem(void)
{
	System *system = &Systems[Current];

	// Entry 0 is the background; the ramp starts one entry in, so the sparsest
	// pixel already carries the system's color rather than fading out of black
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, RETRO_COLORS, system->low, system->high);

	memset(Density, 0, sizeof Density);
	RETRO_Clear();

	// Fit the window to the screen, keeping the plane's aspect
	ScreenScale = MARGIN * MIN(RETRO_WIDTH / (system->right - system->left),
							   RETRO_HEIGHT / (system->top - system->bottom));
	ScreenX = RETRO_WIDTH / 2 - ScreenScale * (system->left + system->right) / 2;
	ScreenY = RETRO_HEIGHT / 2 + ScreenScale * (system->bottom + system->top) / 2;

	float sum = 0;
	for (int i = 0; i < system->transforms; i++) {
		sum += system->transform[i].p;
		Cumulative[i] = sum;
	}

	PointX = 0;
	PointY = 0;
	for (int i = 0; i < TRANSIENT; i++) {
		StepPoint(system);
	}
}

void DEMO_Render2(double time, double deltatime)
{
	static double phase = 0;

	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		phase = 0;
		Current = (Current + 1) % SYSTEMS;
		StartSystem();
	}

	phase += deltatime;
	if (phase > TIME_DRAW + TIME_HOLD) {
		phase = 0;
		Current = (Current + 1) % SYSTEMS;
		StartSystem();
	}

	System *system = &Systems[Current];
	unsigned char *buffer = RETRO_FrameBuffer();

	// Deal points, until the system has had its share of the pass
	int points = phase < TIME_DRAW ? deltatime * POINTS_PER_SECOND : 0;

	for (int i = 0; i < points; i++) {
		StepPoint(system);

		int sx = ScreenX + ScreenScale * PointX;
		int sy = ScreenY - ScreenScale * PointY;

		if (sx >= 0 && sx < RETRO_WIDTH && sy >= 0 && sy < RETRO_HEIGHT) {
			int offset = sy * RETRO_WIDTH + sx;
			if (Density[offset] < DENSITY_MAX) {
				Density[offset]++;
			}
			buffer[offset] = Shade[Density[offset]];
		}
	}

	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// A density ramp, log in the count. Doubling the points that land on a
	// pixel is the same step of the ramp wherever it starts, so the sparse edge
	// of an attractor holds its shape instead of being one shade above nothing
	for (int i = 1; i <= DENSITY_MAX; i++) {
		Shade[i] = 1 + (RETRO_COLORS - 2) * log(1 + i) / log(1 + DENSITY_MAX);
	}

	StartSystem();
}
