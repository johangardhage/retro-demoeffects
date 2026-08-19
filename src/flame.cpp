//
// Flame
//
// A temperature field, one byte per pixel, plus a bed of integer heat along
// the bottom row. Heat is seeded in the bed and the picture is whatever that
// heat becomes after it has risen and cooled. This is not the 8-tap fire of
// fire.cpp (no blur, no self term, no κ∇²T). Each simulation step scatters
// a cell one row up with a 1-pixel x jitter:
//
//   T'(x + j, y − 1) = T(x, y) − random(DECAY)     j ∈ {−1, 0, +1}
//
// if T(x, y) ≥ DECAY, else T'(x, y − 1) = 0. Writes that leave the screen are
// skipped, so a pixel that nobody hits keeps last step's heat, which is the
// smoke that hangs above the fire. The scan is column-major and top to bottom:
// each source is still the previous step (Jacobi in y), and when two columns
// land on the same cell the later, right-hand write wins.
//
// The bed is a separate int per column x ∈ [BED_LEFT, BED_RIGHT). A column
// below MIN_FIRE is still catching and only grows; at or above it, heat is a
// symmetric random walk of half-width ROOT_RAND, plus the intensity bias:
//
//   heat < MIN_FIRE  ∧  heat > TRICKLE :  heat' = heat + random(ignition)
//   heat ≥ MIN_FIRE                    :  heat' = heat + U{−ROOT_RAND, …, ROOT_RAND} + bias
//
// then clamp to [0, 255]. Both ends of the bed are watered with a square bias
// so the fire tapers, and a 3-tap box is run in place (Gauss–Seidel: the left
// neighbour is already the new value). The step is the unit of the rise, so
// the field is advanced in DEMO_Update.
//
// Controls:
//   Return - strike a match
//   + / -  - intensity bias
//   C      - clear the bed
//   W      - douse random columns
//   1–9    - ignition rate (1 = wood, 9 = fastest)
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define BED_LEFT 10
#define BED_RIGHT (RETRO_WIDTH - 10)
#define BED_SPAN (BED_RIGHT - BED_LEFT)

#define ROOT_RAND 20 // half-width of the random walk of a burning column
#define DECAY 4 // max heat lost per rise, so how fast a flame dies
#define MIN_FIRE 50 // below this a column is still catching; at or above, it burns
#define TRICKLE 10 // a column this cold or colder stays out until a match reaches it
#define SMOOTH 1 // radius of the box on the bed, so the tap count is 2·SMOOTH+1
#define MATCH_WIDTH 5
#define MATCH_HEAT 255
#define MATCH_CHANCE 150 // a match is struck with probability 1 / MATCH_CHANCE
#define MATCH_START_STEPS 40
#define WATER_REACH 8.0 // drops land in the outer BED_SPAN / WATER_REACH of each side
#define WATER_COUNT ((int)(BED_SPAN / WATER_REACH))
#define DOUSE_COUNT 10
#define IGNITION_WOOD 3 // keys 1–9 set ignition to IGNITION_WOOD + (n − 1)²
#define FIRE_BIAS_MIN -2
#define FIRE_BIAS_MAX 4
#define FIRE_BIAS_START 1

int FireBed[RETRO_WIDTH];
unsigned char FireBuffer[RETRO_HEIGHT * RETRO_WIDTH];

//
// Advance the field one fixed step
//
// A match writes MATCH_HEAT into MATCH_WIDTH columns of the bed. Each column
// then steps as above, the sides are watered, the bed is box-smoothed, and
// that bed is copied onto the bottom row. The rise scatters every cell one
// row up. The step is the unit of both the rise and the cooling, which is
// why the field is advanced at a fixed rate instead of once per frame.
//
void DEMO_Update(double deltatime)
{
	static int startmatches = MATCH_START_STEPS;
	static int firebias = FIRE_BIAS_START;
	static int ignition = IGNITION_WOOD;

	static const SDL_Scancode ignitionkeys[] = {
		SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
		SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
		SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9
	};

	if (RETRO_KeyPressed(SDL_SCANCODE_MINUS)) {
		if (firebias > FIRE_BIAS_MIN)
			--firebias;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_EQUALS)) {
		if (firebias < FIRE_BIAS_MAX)
			++firebias;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_C)) {
		memset(FireBed, 0, sizeof(FireBed));
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_W)) {
		for (int i = 0; i < DOUSE_COUNT; i++) {
			FireBed[BED_LEFT + RANDOM(BED_SPAN)] = 0;
		}
	}
	for (int n = 0; n < 9; n++) {
		if (RETRO_KeyPressed(ignitionkeys[n])) {
			ignition = IGNITION_WOOD + n * n;
		}
	}

	// Strike a match across MATCH_WIDTH columns of the bed
	if ((RANDOM(MATCH_CHANCE) == 0) || RETRO_KeyPressed(SDL_SCANCODE_RETURN) || startmatches > 0) {
		if (startmatches > 0) {
			--startmatches;
		}
		int x = BED_LEFT + RANDOM(BED_SPAN - MATCH_WIDTH + 1);
		for (int k = 0; k < MATCH_WIDTH; k++) {
			FireBed[x + k] = MATCH_HEAT;
		}
	}

	// Root of the flames
	for (int x = BED_LEFT; x < BED_RIGHT; x++) {
		int heat = FireBed[x];

		if (heat < MIN_FIRE) {
			if (heat > TRICKLE) {
				heat += RANDOM(ignition);
			}
		} else {
			heat += RANDOM(ROOT_RAND * 2 + 1) - ROOT_RAND + firebias;
		}
		FireBed[x] = CLAMP(heat, 0, 256);
	}

	// Water both sides so the fire tapers. U², U ~ [0, 1), has density
	// 1 / (2 √u) on [0, 1), so the drops crowd the ends and the middle of
	// the bed is left to burn.
	for (int i = 0; i < WATER_COUNT; i++) {
		int d = (int)(RAND() * RAND() * BED_SPAN / WATER_REACH);
		FireBed[BED_LEFT + d] = 0;
		FireBed[BED_RIGHT - 1 - d] = 0;
	}

	// Smooth the bed. A (2·SMOOTH+1)-tap box, in place, so the left
	// neighbour is already the new value: Gauss–Seidel, not Jacobi.
	for (int x = BED_LEFT + SMOOTH; x < BED_RIGHT - SMOOTH; x++) {
		int sum = 0;
		for (int k = -SMOOTH; k <= SMOOTH; k++) {
			sum += FireBed[x + k];
		}
		FireBed[x] = sum / (2 * SMOOTH + 1);
	}

	// Seed the bottom row from the bed
	for (int x = BED_LEFT; x < BED_RIGHT; x++) {
		FireBuffer[(RETRO_HEIGHT - 1) * RETRO_WIDTH + x] = (unsigned char)FireBed[x];
	}

	// Rise one row. Each live cell scatters to x + j on the row above and
	// cools by random(DECAY). A dead cell writes 0 onto its own column.
	for (int x = 0; x < RETRO_WIDTH; x++) {
		for (int y = 1; y < RETRO_HEIGHT; y++) {
			int v = FireBuffer[y * RETRO_WIDTH + x];
			if (v < DECAY) {
				FireBuffer[(y - 1) * RETRO_WIDTH + x] = 0;
			} else {
				int dest = x + RANDOM(3) - 1;
				if (dest >= 0 && dest < RETRO_WIDTH) {
					FireBuffer[(y - 1) * RETRO_WIDTH + dest] = v - RANDOM(DECAY);
				}
			}
		}
	}
}

void DEMO_Render(double deltatime)
{
	RETRO_Blit(FireBuffer);
}

void DEMO_Initialize(void)
{
	// Init palette. Index is temperature: mauve smoke, through brick and
	// orange, into gold and a pink-white core.
	RETRO_CreateGradientPalette(0, 32, RETRO_BLACK, RETRO_WINE);
	RETRO_CreateGradientPalette(32, 64, RETRO_WINE, RETRO_BRICK);
	RETRO_CreateGradientPalette(64, 88, RETRO_BRICK, RETRO_SIENNA);
	RETRO_CreateGradientPalette(88, 110, RETRO_SIENNA, RETRO_CARROT);
	RETRO_CreateGradientPalette(110, 128, RETRO_CARROT, RETRO_SAFFRON);
	RETRO_CreateGradientPalette(128, 160, RETRO_SAFFRON, RETRO_JASMINE);
	RETRO_CreateGradientPalette(160, 228, RETRO_JASMINE, RETRO_PINKLACE);
	RETRO_CreateGradientPalette(228, RETRO_COLORS, RETRO_PINKLACE, RETRO_PINKLACE);
}
