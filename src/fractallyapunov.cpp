//
// Lyapunov fractal
//
// A map of the line driven by a rate that is not one number but a rhythm of
// two. Pixel (x, y) is a pair of rates (a, b), and a sequence such as AAB says
// which of them is used at each step:
//
//   x' = f(r_n, x),   r_n = a or b, as the sequence reads, round and round
//
// The picture is the Lyapunov exponent of that orbit, the mean rate at which
// two starts a hair apart separate, which is the mean log slope of the map
// along the orbit:
//
//   λ = (1/N) Σ log |f'(r_n, x_n)|
//
// Below zero the orbit is drawn onto a cycle and nearby starts close up: order.
// Above zero they part: chaos. λ falls to −∞ where the cycle passes through a
// point where the slope is zero, and those are the curves that cross each
// other without meeting.
//
// Four maps are used. The logistic map, started at x = 1/2:
//
//   f = r x (1 − x)                        f' = r (1 − 2x)
//
// the same with its left half shifted, so that it breaks at its maximum:
//
//   f = r x (1 − x) + (α − 1)(r − 2) / 4   if x ≤ 1/2
//
// a sine map, started at x = 0, in which r is a phase and the picture
// therefore repeats every π in a and in b:
//
//   f = B sin²(x + r)                      f' = B sin 2(x + r)
//
// and the same with a step added on every other half period of x:
//
//   f = B sin²(x + r) + β r                if x mod π ≥ π/2
//
// The sum is carried on, not finished. A pixel keeps its x and its sum between
// frames, and every fixed step adds STEPS steps of the map to both, their
// slopes multiplied together and logged once. So a slide comes up as a blur,
// the exponent of a few steps, and sharpens over DRAW_UPDATES fixed steps as N
// grows, and is then held. The first WARMUP_UPDATES move x and are not
// counted, the start being nowhere near the orbit's final course.
//
// The slides are the pictures of the Wikipedia article on the fractal, in its
// order and its colors: the sequences AB, AB again at the swallow, AABAB and
// BBBBBBAAAAAA, whose corner is Zircon Zity, and then eight by Mario Markus,
// who found the fractal. The article gives the map and the sequence of those
// eight but not the window, nor α above, nor the step of the fourth map in
// full; the windows and those constants are chosen here to bring up figures of
// the same kind. They are numbered as their files there are. A window with a
// turn has the diagonal a = b upright, which is the axis the symmetric
// sequences mirror in. The article's own four run b across and a up; here a
// always runs across, so their sequences are written with the letters
// exchanged.
//
// Chaos is shaded by λ and order by √(−λ), which spends the ramp near the
// border, where most of a picture lies. Entry 0 is an orbit that has left for
// infinity, as the broken map's can.
//
// Tab deals the next slide.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define STEPS 8 // steps of the map a fixed step
#define WARMUP_UPDATES 8 // fixed steps that move x before the sum starts
#define DRAW_UPDATES 360 // fixed steps a slide is summed for
#define HOLD_UPDATES 240 // fixed steps the finished slide is held
#define SLOPE_FLOOR 1.0e-30f // the least a fixed step's product of slopes counts as
#define CHAOS_COLORS 64 // palette entries of chaos, after entry 0; the rest are order

enum {
	MAP_LOGISTIC,
	MAP_BROKEN_LOGISTIC,
	MAP_SINE,
	MAP_STEPPED_SINE
};

struct Slide {
	int map;
	float gain; // B of the sine maps
	float shift; // α of the broken logistic map, β of the stepped sine map
	const char *sequence;
	float a, b; // the center of the window
	float halfwidth, halfheight; // its reach across and up the screen
	float turn; // degrees it is turned; at 0 a runs across and b up
	float orderlimit; // −λ that reaches the end of the order ramp
	float chaoslimit; // λ that reaches the end of the chaos ramp
	RETRO_Palette order[3]; // order at the border, half way, and at the limit
	RETRO_Palette chaos[2]; // chaos at the border and at the limit
};

static Slide Slides[] = {
	// The sequence AB
	{ MAP_LOGISTIC, 0, 0, "BA", 3.0f, 3.0f, 1.0f, 1.0f, 0, 2.0f, 0.4f,
	  { { 255, 215, 0 }, { 200, 140, 0 }, { 0, 0, 0 } }, { { 0, 0, 0 }, { 0, 0, 255 } } },
	// The swallow
	{ MAP_LOGISTIC, 0, 0, "BA", 3.84f, 3.84f, 0.03f, 0.03f, 0, 2.0f, 0.4f,
	  { { 230, 240, 0 }, { 160, 170, 0 }, { 0, 0, 0 } }, { { 0, 0, 0 }, { 60, 0, 200 } } },
	// The sequence AABAB
	{ MAP_LOGISTIC, 0, 0, "BBABA", 3.0f, 3.0f, 1.0f, 1.0f, 0, 2.0f, 0.4f,
	  { { 255, 215, 0 }, { 200, 140, 0 }, { 0, 0, 0 } }, { { 0, 0, 0 }, { 0, 0, 255 } } },
	// Zircon Zity
	{ MAP_LOGISTIC, 0, 0, "AAAAAABBBBBB", 2.95f, 3.7f, 0.45f, 0.3f, 0, 2.0f, 0.4f,
	  { { 255, 215, 0 }, { 200, 140, 0 }, { 0, 0, 0 } }, { { 0, 0, 0 }, { 0, 0, 255 } } },
	// Markus 3
	{ MAP_BROKEN_LOGISTIC, 0, 0.3f, "AB", 3.1f, 3.1f, 0.9f, 0.675f, 135, 3.0f, 0.7f,
	  { { 170, 225, 240 }, { 40, 120, 180 }, { 10, 30, 80 } }, { { 130, 200, 225 }, { 60, 130, 180 } } },
	// Markus 6
	{ MAP_SINE, 1.95f, 0, "AAAAAABBBBBB", 0.652f, 0.652f, 0.42f, 0.315f, -45, 2.0f, 1.0f,
	  { { 255, 200, 20 }, { 210, 110, 0 }, { 50, 15, 0 } }, { { 0, 0, 0 }, { 30, 22, 10 } } },
	// Markus 7
	{ MAP_STEPPED_SINE, 2.3f, 0.2f, "AAABBB", 1.125f, 1.125f, 0.6f, 0.45f, -45, 2.0f, 1.0f,
	  { { 225, 205, 70 }, { 120, 110, 30 }, { 10, 10, 0 } }, { { 130, 65, 10 }, { 60, 25, 5 } } },
	// Markus 8
	{ MAP_STEPPED_SINE, 2.36f, 0.16f, "AAAAAAABBBBBBB", 1.6f, 1.0f, 0.45f, 0.34f, -25, 2.0f, 1.0f,
	  { { 255, 200, 0 }, { 230, 120, 0 }, { 90, 30, 0 } }, { { 0, 140, 60 }, { 0, 50, 25 } } },
	// Markus 5
	{ MAP_SINE, 2.8f, 0, "AB", 4.712f, 3.534f, 4.712f, 3.534f, 0, 2.0f, 1.0f,
	  { { 245, 215, 0 }, { 170, 140, 0 }, { 30, 25, 0 } }, { { 25, 25, 25 }, { 0, 0, 0 } } },
	// Markus 4
	{ MAP_SINE, 2.7f, 0, "AAAAAABBBBBB", 0.814f, 0.814f, 0.5f, 0.375f, 135, 2.5f, 1.0f,
	  { { 70, 30, 0 }, { 245, 170, 30 }, { 255, 245, 190 } }, { { 0, 150, 190 }, { 0, 60, 90 } } },
	// Markus 2
	{ MAP_LOGISTIC, 0, 0, "BBBBBBBAABBBBBBBBBBABABABABABABABABAAAAAAAABBAAAAAAA", 3.75f, 2.6f, 0.24f, 0.18f, 0, 1.0f, 0.7f,
	  { { 255, 225, 0 }, { 200, 30, 0 }, { 40, 0, 0 } }, { { 90, 10, 0 }, { 20, 0, 0 } } },
	// Markus 1
	{ MAP_LOGISTIC, 0, 0, "ABAABBAAABBBAABBAB", 3.05f, 3.55f, 0.24f, 0.18f, 0, 1.0f, 0.7f,
	  { { 20, 110, 30 }, { 0, 60, 20 }, { 0, 10, 0 } }, { { 240, 235, 0 }, { 120, 150, 0 } } },
};

#define SLIDES ((int)(sizeof(Slides) / sizeof(Slides[0])))

static float X[RETRO_WIDTH * RETRO_HEIGHT];
static float Sum[RETRO_WIDTH * RETRO_HEIGHT];
static int Position; // of the next step in the sequence
static int Updates; // fixed steps the slide has been summed and held for
static int Current = 0;

//
// Carry every pixel's orbit STEPS steps along the sequence, adding the log of
// the slopes it met to the pixel's sum
//
static void StepSlide(void)
{
	Slide *slide = &Slides[Current];

	int length = strlen(slide->sequence);
	float cosine = cos(radians(slide->turn));
	float sine = sin(radians(slide->turn));

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float v = slide->halfheight * (1 - (y + 0.5f) * 2 / RETRO_HEIGHT);

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float u = slide->halfwidth * ((x + 0.5f) * 2 / RETRO_WIDTH - 1);

			// The window, turned about its center
			float a = slide->a + u * cosine - v * sine;
			float b = slide->b + u * sine + v * cosine;

			int offset = y * RETRO_WIDTH + x;
			float value = X[offset];
			float slope = 1;

			for (int i = 0; i < STEPS; i++) {
				float rate = slide->sequence[(Position + i) % length] == 'A' ? a : b;

				if (slide->map == MAP_LOGISTIC || slide->map == MAP_BROKEN_LOGISTIC) {
					bool left = value <= 0.5f;

					slope *= fabsf(rate * (1 - 2 * value));
					value = rate * value * (1 - value);
					if (slide->map == MAP_BROKEN_LOGISTIC && left) {
						value += (slide->shift - 1) * (rate - 2) / 4;
					}
				} else {
					bool upper = fract(value / (float)M_PI) >= 0.5f;
					float angle = value + rate;

					slope *= fabsf(slide->gain * sinf(2 * angle));
					value = slide->gain * sinf(angle) * sinf(angle);
					if (slide->map == MAP_STEPPED_SINE && upper) {
						value += slide->shift * rate;
					}
				}
			}

			X[offset] = value;
			Sum[offset] += logf(MAX(slope, SLOPE_FLOOR));
		}
	}

	Position = (Position + STEPS) % length;
}

//
// Set the slide's palette, put every orbit back at its start, run the fixed
// steps that are not counted, and empty the sums
//
static void StartSlide(void)
{
	Slide *slide = &Slides[Current];

	// Entry 0 is an orbit that has left for infinity. Chaos runs from the
	// border out, then order from the border out, in two ramps
	int half = (CHAOS_COLORS + 1 + RETRO_COLORS) / 2;
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, CHAOS_COLORS + 1, slide->chaos[0], slide->chaos[1]);
	RETRO_CreateGradientPalette(CHAOS_COLORS + 1, half, slide->order[0], slide->order[1]);
	RETRO_CreateGradientPalette(half, RETRO_COLORS, slide->order[1], slide->order[2]);

	bool logistic = slide->map == MAP_LOGISTIC || slide->map == MAP_BROKEN_LOGISTIC;
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		X[i] = logistic ? 0.5f : 0;
	}

	Position = 0;
	for (int i = 0; i < WARMUP_UPDATES; i++) {
		StepSlide();
	}

	memset(Sum, 0, sizeof(Sum));
	Updates = 0;
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	if (Updates == DRAW_UPDATES + HOLD_UPDATES) {
		Current = (Current + 1) % SLIDES;
		StartSlide();
	}

	// Past DRAW_UPDATES the count goes on but the sum is left alone: the hold
	if (Updates < DRAW_UPDATES) {
		StepSlide();
	}
	Updates++;
}

void DEMO_Render(RETRO_Time time)
{
	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		Current = (Current + 1) % SLIDES;
		StartSlide();
	}

	Slide *slide = &Slides[Current];

	// The steps the sums hold so far
	int steps = MIN(Updates, DRAW_UPDATES) * STEPS;
	if (steps == 0) {
		return;
	}

	unsigned char *buffer = RETRO_FrameBuffer();
	int ordercolors = RETRO_COLORS - CHAOS_COLORS - 1;

	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		float lambda = Sum[i] / steps;

		// An orbit that has left has no exponent, and no comparison holds for it
		if (lambda > 0) {
			buffer[i] = 1 + CLAMP(lambda * CHAOS_COLORS / slide->chaoslimit, 0, CHAOS_COLORS);
		} else if (lambda <= 0) {
			buffer[i] = CHAOS_COLORS + 1 + CLAMP(sqrtf(-lambda / slide->orderlimit) * ordercolors, 0, ordercolors);
		}
	}
}

void DEMO_Initialize(void)
{
	StartSlide();
}
