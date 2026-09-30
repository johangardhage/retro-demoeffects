//
// L-system
//
// Four classic curves drawn by a turtle that reads a string. A curve is an
// axiom and a rule or two that rewrite a letter into a longer string,
//
//   Koch snowflake:   F++F++F     F → F-F++F-F
//   Heighway dragon:  F           F → F+G           G → F-G
//   Hilbert curve:    A           A → +BF-AFA-FB+   B → -AF+BFB+FA-
//   Gosper curve:     F           F → F-G--G+F++FF+G-
//                                 G → +F-GG--G-F++F+G
//
// and generation n is the axiom with every letter rewritten n times over. The
// turtle then reads the result a symbol at a time:
//
//   F, G   a step forward, drawing
//   +, -   a turn left or right, by one of the TURNS a full circle is cut into
//
// and passes over any other letter, which is there only to be rewritten. The
// string is never built. A letter with a rule and generations still to go is
// read as its rule, one generation down, so the whole string is walked by
// recursion, in order, and all that is kept is the path the turtle took.
//
// The path is fitted to the screen, so the snowflake holds its size while its
// edge grows by 4/3 a generation, and the Hilbert and Gosper curves, which in
// the limit pass through every point of a square and of a hexagon with a
// fractal edge, hold theirs while the mesh gets finer. Each generation is
// traced over TIME_DRAW seconds and held for TIME_HOLD, shaded along its length
// so the order the turtle went in stays readable once it is done.
//
// Tab deals the next curve.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define MAX_RULES 2
#define MAX_POINTS 4100 // the longest path, 4^6 points, and a little
#define TIME_DRAW 1.5 // seconds a generation is traced for
#define TIME_HOLD 1.0 // seconds the finished generation is held
#define MARGIN 0.92 // of the screen the fitted path fills

struct Rule {
	char symbol;
	const char *production;
};

struct Curve {
	const char *axiom;
	Rule rule[MAX_RULES];
	int rules;
	int turns; // turns in a full circle: + and - are one each
	int generations; // the last one drawn, the finest the screen resolves
	RETRO_Palette first, last; // the ramp the path is shaded along
};

static Curve Curves[] = {
	{ "F++F++F", { { 'F', "F-F++F-F" } }, 1, 6, 4, RETRO_AZURE, RETRO_WHITE }, // Koch snowflake
	{ "F", { { 'F', "F+G" }, { 'G', "F-G" } }, 2, 4, 12, RETRO_SCARLET, RETRO_GOLD }, // Heighway dragon
	{ "A", { { 'A', "+BF-AFA-FB+" }, { 'B', "-AF+BFB+FA-" } }, 2, 4, 6, RETRO_PURPLE, RETRO_CYAN }, // Hilbert curve
	{ "F", { { 'F', "F-G--G+F++FF+G-" }, { 'G', "+F-GG--G-F++F+G" } }, 2, 6, 4, RETRO_SPRINGGREEN, RETRO_YELLOW }, // Gosper curve
};

#define CURVES ((int)(sizeof(Curves) / sizeof(Curves[0])))

struct Point {
	float x, y;
};

static Point Path[MAX_POINTS];
static int Points;
static int Heading; // in turns of the curve, counterclockwise from +x
static float ScreenScale, ScreenX, ScreenY;
static int Current = 0;
static int Generation = 1;

//
// Read a string with the turtle, generations rewrites deep
//
static void TraceString(const char *string, int generations)
{
	Curve *curve = &Curves[Current];

	for (const char *symbol = string; *symbol; symbol++) {
		// A letter with a rule is its rule, while there are generations to go
		const char *production = NULL;
		for (int i = 0; i < curve->rules && generations > 0; i++) {
			if (curve->rule[i].symbol == *symbol) {
				production = curve->rule[i].production;
			}
		}

		if (production) {
			TraceString(production, generations - 1);
		} else if (*symbol == '+') {
			Heading = (Heading + 1) % curve->turns;
		} else if (*symbol == '-') {
			Heading = (Heading + curve->turns - 1) % curve->turns;
		} else if ((*symbol == 'F' || *symbol == 'G') && Points < MAX_POINTS) {
			float angle = Heading * 2 * M_PI / curve->turns;
			Path[Points].x = Path[Points - 1].x + cos(angle);
			Path[Points].y = Path[Points - 1].y + sin(angle);
			Points++;
		}
	}
}

//
// Walk the current generation of the current curve, and frame the path
//
static void StartGeneration(void)
{
	Curve *curve = &Curves[Current];

	// Entry 0 is the background; the ramp is the rest
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, RETRO_COLORS, curve->first, curve->last);

	Path[0].x = 0;
	Path[0].y = 0;
	Points = 1;
	Heading = 0;
	TraceString(curve->axiom, Generation);

	// The box the path fills
	float left = 0, right = 0, bottom = 0, top = 0;
	for (int i = 0; i < Points; i++) {
		left = MIN(left, Path[i].x);
		right = MAX(right, Path[i].x);
		bottom = MIN(bottom, Path[i].y);
		top = MAX(top, Path[i].y);
	}

	// Fit the box to the screen, keeping the plane's aspect. A path with no
	// height is as tall as one step
	ScreenScale = MARGIN * MIN(RETRO_WIDTH / MAX(right - left, 1.0f),
							   RETRO_HEIGHT / MAX(top - bottom, 1.0f));
	ScreenX = RETRO_WIDTH / 2.0 - ScreenScale * (left + right) / 2;
	ScreenY = RETRO_HEIGHT / 2.0 + ScreenScale * (bottom + top) / 2;
}

void DEMO_Render(double time, double deltatime)
{
	static double phase = 0;

	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		phase = 0;
		Current = (Current + 1) % CURVES;
		Generation = 1;
		StartGeneration();
	}

	// Calculate phase. One generation traced and held, then the next, and the
	// next curve after the last
	phase += deltatime;
	if (phase > TIME_DRAW + TIME_HOLD) {
		phase = 0;
		Generation++;
		if (Generation > Curves[Current].generations) {
			Current = (Current + 1) % CURVES;
			Generation = 1;
		}
		StartGeneration();
	}

	// Draw as much of the path as the turtle has traced
	int steps = Points - 1;
	int traced = steps * MIN(phase / TIME_DRAW, 1.0);

	for (int i = 0; i < traced; i++) {
		int x1 = lround(ScreenX + ScreenScale * Path[i].x);
		int y1 = lround(ScreenY - ScreenScale * Path[i].y);
		int x2 = lround(ScreenX + ScreenScale * Path[i + 1].x);
		int y2 = lround(ScreenY - ScreenScale * Path[i + 1].y);
		unsigned char color = 1 + i * (RETRO_COLORS - 2) / steps;

		RETRO_DrawLine(x1, y1, x2, y2, color);
	}
}

void DEMO_Initialize(void)
{
	StartGeneration();
}
