//
// Color cycling
//
// A still picture that moves only through its palette. The picture is never
// redrawn: waterfalls, streams and mist are painted in bands of palette
// entries laid out along their motion, and rotating a band walks the colors
// along it. The scene is Mark J. Ferrari's Jungle Waterfall, a 640×480
// Deluxe Paint picture, with the cycle ranges it was painted for.
//
// A range [low, high] of n entries turns at rate / 280 entries a second, so
// at time t it has moved
//
//   shift = t · rate / 280   (mod n)
//
// entries toward high, the last one wrapping around to low. The whole part
// of the shift rotates the entries, and the fraction blends each entry with
// the one behind it, so the colors glide rather than step:
//
//   color_i = mix(base[(i − ⌊shift⌋) mod n],
//                 base[(i − ⌊shift⌋ − 1) mod n], fract(shift))
//
// A range marked reverse runs the other way. A ping-pong range runs to the
// end and back, and a sine range swings back and forth by a quarter or half
// of its length, as Deluxe Paint's cycle modes do.
//
// Tab steps every range on to its next mode, in the order forward, reverse,
// ping-pong, quarter sine, half sine, and back to the mode it was painted for.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_WIDTH 640
#define RETRO_HEIGHT 480

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define CYCLE_UNITS 280.0 // rate units per entry a second

enum CycleMode { CYCLE_FORWARD, CYCLE_REVERSE, CYCLE_PINGPONG, CYCLE_SINE_QUARTER, CYCLE_SINE_HALF, CYCLE_MODES };

struct CycleRange {
	int low, high; // palette entries, both included
	int rate;
	CycleMode mode;
};

static const CycleRange Cycles[] = {
	{ 135, 143, 1536, CYCLE_FORWARD },
	{ 127, 134, 1380, CYCLE_FORWARD },
	{ 119, 126, 2304, CYCLE_FORWARD },
	{ 217, 223, 1536, CYCLE_FORWARD },
	{ 210, 216, 2841, CYCLE_FORWARD },
	{ 203, 209, 2841, CYCLE_FORWARD },
	{ 196, 202, 2841, CYCLE_FORWARD },
	{ 189, 195, 2841, CYCLE_FORWARD },
	{ 182, 188, 2841, CYCLE_FORWARD },
	{ 175, 181, 2841, CYCLE_FORWARD }
};

static RETRO_Image *Picture;
static int ModeStep; // modes every range has been stepped on by Tab

//
// How far a range has turned at this time, in entries toward high
//
static double Shift(const CycleRange &range, double time)
{
	int n = range.high - range.low + 1;
	double steps = time * range.rate / CYCLE_UNITS;

	switch ((range.mode + ModeStep) % CYCLE_MODES) {
	case CYCLE_REVERSE:
		return -fmod(steps, n);
	case CYCLE_PINGPONG: {
		double swing = fmod(steps, 2 * n);
		return swing < n ? swing : 2 * n - swing;
	}
	case CYCLE_SINE_QUARTER:
		return (sin(2 * M_PI * fmod(steps, n) / n) + 1) * n / 4;
	case CYCLE_SINE_HALF:
		return (sin(2 * M_PI * fmod(steps, n) / n) + 1) * n / 2;
	default:
		return fmod(steps, n);
	}
}

void DEMO_Render(RETRO_Time time)
{
	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		ModeStep = (ModeStep + 1) % CYCLE_MODES;
	}

	RETRO_Palette *base = Picture->palette;

	for (const CycleRange &range : Cycles) {
		int n = range.high - range.low + 1;
		double shift = Shift(range, time.total);
		int whole = floor(shift);
		float blend = shift - whole;

		for (int i = 0; i < n; i++) {
			RETRO_Palette from = base[range.low + WRAP(i - whole, n)];
			RETRO_Palette to = base[range.low + WRAP(i - whole - 1, n)];
			RETRO_SetColor(range.low + i, mix(from, to, blend));
		}
	}

	RETRO_Blit(Picture->data);
}

void DEMO_Initialize(void)
{
	Picture = RETRO_LoadImage("assets/junglewaterfall_640x480.pcx", true);
}
