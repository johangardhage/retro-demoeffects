//
// Sun
//
// A flare drawn in polar coordinates. The effect lives in a strip of
// FLARE_WIDTH angular bins by FLARE_RADII radial bins, one row per radius, and
// the screen is only a lookup into it: a pixel reads the bin its own (angle,
// radius) falls in. The strip itself is the rising-fire cellular automaton
// turned inside out - a ring of noise is seeded into one row near the centre
// and RETRO_Blur lifts it toward row 0, which is the rim, so the flames crawl
// outward instead of upward.
//
// A pixel is sampled at its own centre, (dx, dy) = (ix + 1/2, iy + 1/2), so the
// origin falls on the corner shared by the four middle pixels and the map is
// symmetric under (dx, dy) -> (-dx, -dy). The angle is a full turn measured
// from +y,
//
//   a = FLARE_WIDTH * (atan2(dx, dy) + pi) / 2pi        in [0, FLARE_WIDTH)
//
// rather than atan(dx / dy), which is 180-degree periodic and would fold the
// flare onto itself. The radius is a straight ramp, reversed so that row 0 is
// the rim:
//
//   r = (FLARE_RADII - 1) * (1 - |(dx, dy)| / rmax)     in [0, FLARE_RADII)
//
// with rmax the distance to the outermost pixel centre, a corner, so the
// corners land exactly on row 0 and no radial bin is wasted.
//
// The seeded ring is a fixed pattern of noise, swung back and forth by
//
//   swirl  = (FLARE_SWIRL + FLARE_EBB) / 2 + (FLARE_SWIRL - FLARE_EBB) / 2 * SIN(phase)
//   offset = swirl * SIN(SUN_SWIRLS * phase)
//
// a fast swing whose width a slower swell walks between FLARE_EBB and FLARE_SWIRL
// bins. Written as a bare product of the two sines the swell would instead run from
// -FLARE_SWIRL to FLARE_SWIRL, and passing through zero costs twice a turn: the
// swing shrinks away until the ring sits still at neutral, then opens back up
// inverted, because the width came out the far side with the opposite sign. Held
// off zero by FLARE_EBB it keeps its sign and its stroke, so the ring still
// reverses - a bounded offset has to - but it turns over briskly every time
// instead of unwinding and starting over. Both live on phase, an angle in the
// library's RETRO_SINCOS_ANGLE units per turn, wrapping on one turn of the swell.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define FLARE_WIDTH RETRO_WIDTH // angular bins, one per column; RETRO_Blur strides a row by RETRO_WIDTH, so the strip is that wide
#define FLARE_RADII 120 // radial bins, one per row, at most RETRO_HEIGHT
#define FLARE_HOLE 2 // bins inside the seeded row that nothing ever reaches
#define FLARE_SEED (FLARE_RADII - FLARE_HOLE - 1) // row the ring of noise is seeded into
#define FLARE_SWIRL 32 // angular bins the ring swings either way where the swell is widest
#define FLARE_EBB 8 // and where it is narrowest; kept off zero, so the swing never dies out or flips sign
#define FLARE_DECAY 1 // subtracted after the 7-tap average, so how fast a flame dies as it travels out
#define SUN_SPEED 30.0 // phase units per second; the original +0.5 per frame at 60 Hz
#define SUN_SWIRLS 8 // swings the ring makes per turn of the swell
#define SUN_RAMP 128 // heat the palette ramp spans; the flare never quite reaches it, and anything hotter is white

unsigned char Flare[FLARE_WIDTH];
int FlareOffset[RETRO_WIDTH * RETRO_HEIGHT];
unsigned char LightMap[RETRO_WIDTH * RETRO_HEIGHT]; // the strip is its top FLARE_RADII rows, but RETRO_Blur wants a full buffer

//
// Advance the flare one fixed step
//
// The ring of noise is written into row FLARE_SEED and the blur then replaces every
// cell with the mean of seven neighbours - three copies of the cell one row inward,
// one two rows inward, and three side by side three rows inward - less FLARE_DECAY:
//
//   L'(a, r) = max(0, (3*L(a, r+1) + L(a, r+2)
//                    + L(a-1, r+3) + L(a, r+3) + L(a+1, r+3)) / 7 - FLARE_DECAY)
//
// Every tap sits inward of the cell it feeds, so each step carries the ring one row
// out toward the rim, and the three-wide tap smears a ray into its neighbours. The
// weights sum to the tap count, so a flat field is carried unchanged and only
// FLARE_DECAY cools it: a ray fades over 255 / FLARE_DECAY steps at the most, and
// sooner where it averages with darker neighbours beside it.
//
// The blur wraps both axes. Wrapping the angle is what makes the strip a full turn,
// joining bin FLARE_WIDTH - 1 to bin 0 so a ray crosses the seam instead of ending
// at it. Wrapping the radius only reaches the rows past FLARE_RADII, which no pixel
// ever reads.
//
// Seeding before the blur means the blur overwrites row FLARE_SEED in the same pass
// that lifts it outward, so that row renders black. It is the hidden fuel bed, and
// with the FLARE_HOLE rows inside it that is what the dark core is made of.
//
// The step is the unit of both the travel and the cooling, which is why the flare is
// advanced at a fixed rate instead of once per frame.
//
void DEMO_FixedUpdate(double timestep)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + timestep * SUN_SPEED, RETRO_SINCOS_ANGLE);

	// Seed the ring of noise, swinging back and forth by a width the swell narrows but never closes
	double swirl = (FLARE_SWIRL + FLARE_EBB) / 2.0 + (FLARE_SWIRL - FLARE_EBB) / 2.0 * SIN(phase);
	int offset = swirl * SIN(phase * SUN_SWIRLS);
	for (int a = 0; a < FLARE_WIDTH; a++) {
		LightMap[FLARE_SEED * FLARE_WIDTH + a] = Flare[WRAP(offset + a, FLARE_WIDTH)];
	}

	// Blur strip
	RETRO_Blur(RETRO_BLUR_FLAME, FLARE_DECAY, RETRO_BLUR_WRAP, LightMap);
}

void DEMO_Render(double time, double deltatime)
{
	// Draw sun, each pixel reading the bin its angle and radius fall in
	for (int iy = 0; iy < RETRO_HEIGHT; iy++) {
		for (int ix = 0; ix < RETRO_WIDTH; ix++) {
			unsigned char color = LightMap[FlareOffset[iy * RETRO_WIDTH + ix]];

			RETRO_PutPixel(ix, iy, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Index is heat, so the ramp is the cooling curve, run over SUN_RAMP
	// rather than all 256 indices: the flare spends most of its area under a quarter of
	// the range and only the rays near the core climb out of it, so a ramp spread over
	// the whole byte would never leave the reds. Anything hotter than the ramp is white
	RETRO_CreateGradientPalette(0, SUN_RAMP / 8, RETRO_BLACK, RETRO_DARKRED);
	RETRO_CreateGradientPalette(SUN_RAMP / 8, SUN_RAMP / 4, RETRO_DARKRED, RETRO_RED);
	RETRO_CreateGradientPalette(SUN_RAMP / 4, SUN_RAMP * 3 / 8, RETRO_RED, RETRO_ORANGE);
	RETRO_CreateGradientPalette(SUN_RAMP * 3 / 8, SUN_RAMP * 5 / 8, RETRO_ORANGE, RETRO_YELLOW);
	RETRO_CreateGradientPalette(SUN_RAMP * 5 / 8, SUN_RAMP, RETRO_YELLOW, RETRO_WHITE);
	for (int i = SUN_RAMP; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, RETRO_WHITE);
	}

	// Init flare offset table, one strip bin per pixel
	double rmax = hypot(RETRO_WIDTH / 2 - 0.5, RETRO_HEIGHT / 2 - 0.5);
	int offset = 0;

	for (int iy = -RETRO_HEIGHT / 2; iy < RETRO_HEIGHT / 2; iy++) {
		for (int ix = -RETRO_WIDTH / 2; ix < RETRO_WIDTH / 2; ix++, offset++) {
			double dx = ix + 0.5;
			double dy = iy + 0.5;
			int a = WRAP(FLARE_WIDTH * (atan2(dx, dy) + M_PI) / (2 * M_PI), FLARE_WIDTH);
			int r = CLAMP((FLARE_RADII - 1) * (1 - hypot(dx, dy) / rmax), 0, FLARE_RADII);

			FlareOffset[offset] = r * FLARE_WIDTH + a;
		}
	}

	// Init flare
	for (int a = 0; a < FLARE_WIDTH; a++) {
		Flare[a] = RANDOM(RETRO_COLORS);
	}
}
