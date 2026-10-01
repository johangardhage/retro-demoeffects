//
// Reaction-diffusion
//
// Two chemicals on a grid, one pixel a cell, in the Gray-Scott model. A
// feeds the reaction and B is made from it:
//
//   A' = A + DIFFUSION_A ∇²A − A B² + F (1 − A)
//   B' = B + DIFFUSION_B ∇²B + A B² − (F + k) B
//
// A B² turns A into B wherever they meet, F tops A back up toward 1 and
// F + k drains B away. B spreads half as fast as A, so a patch of B eats the
// A around it faster than A can flow back in, and it grows by budding and
// splitting instead of spreading as a smooth blob. ∇² is the 3×3 kernel
// with 0.2 on the sides, 0.05 on the corners and −1 in the middle, and the
// grid wraps at the edges. ITERATIONS steps run per fixed update.
//
// Feed and kill drift slowly between two regimes, coral (F, k) =
// (CORAL_FEED, CORAL_KILL), where B grows into branching worms that fill
// the screen, and mitosis, (MITOSIS_FEED, MITOSIS_KILL), where it breaks
// into spots that keep dividing. The pattern therefore never settles.
// The grid starts full of A with SEEDS discs of B scattered over it.
//
// The shade is B, embossed by its slope along the diagonal,
//
//   s = B / B_MAX + EMBOSS (B(x − 1, y − 1) − B(x + 1, y + 1))
//
// so the ridges look lit from the top left, and s picks a gradient from
// deep blue through teal to warm white.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define DIFFUSION_A 1.0f
#define DIFFUSION_B 0.5f
#define CORAL_FEED 0.0545f
#define CORAL_KILL 0.062f
#define MITOSIS_FEED 0.0367f
#define MITOSIS_KILL 0.0649f
#define DRIFT_PERIOD 60.0 // seconds for feed and kill to go and come back
#define ITERATIONS 8 // model steps per fixed update
#define SEEDS 12
#define SEED_RADIUS 5 // cells
#define B_MAX 0.4f // the B that the shade calls full
#define EMBOSS 2.5f // weight of the slope in the shade

static float A[2][RETRO_HEIGHT][RETRO_WIDTH];
static float B[2][RETRO_HEIGHT][RETRO_WIDTH];
static int Current;

//
// One Gray-Scott step from grid Current into the other
//
static void Step(float feed, float kill)
{
	float (*a)[RETRO_WIDTH] = A[Current];
	float (*b)[RETRO_WIDTH] = B[Current];
	float (*nexta)[RETRO_WIDTH] = A[1 - Current];
	float (*nextb)[RETRO_WIDTH] = B[1 - Current];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int up = WRAPHEIGHT(y - 1);
		int down = WRAPHEIGHT(y + 1);

		for (int x = 0; x < RETRO_WIDTH; x++) {
			int left = WRAPWIDTH(x - 1);
			int right = WRAPWIDTH(x + 1);

			float laplacea = 0.2f * (a[y][left] + a[y][right] + a[up][x] + a[down][x])
				+ 0.05f * (a[up][left] + a[up][right] + a[down][left] + a[down][right]) - a[y][x];
			float laplaceb = 0.2f * (b[y][left] + b[y][right] + b[up][x] + b[down][x])
				+ 0.05f * (b[up][left] + b[up][right] + b[down][left] + b[down][right]) - b[y][x];

			float reaction = a[y][x] * b[y][x] * b[y][x];
			nexta[y][x] = CLAMP01(a[y][x] + DIFFUSION_A * laplacea - reaction + feed * (1 - a[y][x]));
			nextb[y][x] = CLAMP01(b[y][x] + DIFFUSION_B * laplaceb + reaction - (feed + kill) * b[y][x]);
		}
	}

	Current = 1 - Current;
}

//
// Run the model in fixed steps, so how fast the pattern grows follows the
// step rate rather than the frame rate
//
void DEMO_FixedUpdate(double timestep)
{
	// Calculate phase. Feed and kill ease from coral to mitosis and back
	static double phase = 0;
	phase = fract(phase + timestep / DRIFT_PERIOD);

	float t = 0.5f - 0.5f * cos(2 * M_PI * phase);
	float feed = mix(CORAL_FEED, MITOSIS_FEED, t);
	float kill = mix(CORAL_KILL, MITOSIS_KILL, t);

	for (int i = 0; i < ITERATIONS; i++) {
		Step(feed, kill);
	}
}

void DEMO_Render(double time, double deltatime)
{
	float (*b)[RETRO_WIDTH] = B[Current];
	unsigned char *buffer = RETRO_FrameBuffer();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int up = WRAPHEIGHT(y - 1);
		int down = WRAPHEIGHT(y + 1);

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float slope = b[up][WRAPWIDTH(x - 1)] - b[down][WRAPWIDTH(x + 1)];
			float shade = b[y][x] / B_MAX + EMBOSS * slope;
			*buffer++ = CLAMP(shade * (RETRO_COLORS - 1), 0, RETRO_COLORS);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 90, RETRO_Palette{ 4, 8, 30 }, RETRO_DEEPSEABLUE);
	RETRO_CreateGradientPalette(90, 180, RETRO_DEEPSEABLUE, RETRO_TEAL);
	RETRO_CreateGradientPalette(180, 230, RETRO_TEAL, RETRO_JASMINE);
	RETRO_CreateGradientPalette(230, RETRO_COLORS, RETRO_JASMINE, RETRO_WHITE);

	// Fill the grid with A, then scatter discs of B over it
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			A[Current][y][x] = 1;
			B[Current][y][x] = 0;
		}
	}

	for (int seed = 0; seed < SEEDS; seed++) {
		int cx = RANDOM(RETRO_WIDTH);
		int cy = RANDOM(RETRO_HEIGHT);

		for (int dy = -SEED_RADIUS; dy <= SEED_RADIUS; dy++) {
			for (int dx = -SEED_RADIUS; dx <= SEED_RADIUS; dx++) {
				if (dx * dx + dy * dy <= SEED_RADIUS * SEED_RADIUS) {
					B[Current][WRAPHEIGHT(cy + dy)][WRAPWIDTH(cx + dx)] = 1;
				}
			}
		}
	}
}
