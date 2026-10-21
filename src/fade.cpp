//
// Fade
//
// A still picture whose palette is scaled. The pixels never change; only
// the DAC does:
//
//   C(s) = s · C_loaded          fading in
//   C(s) = (1 − s) · C_loaded    fading out
//
// s runs from 0 to 1 over TIME_FADEIN / TIME_FADEOUT. The cycle holds
// for TIME_HOLD on black, fades in, holds for TIME_HOLD on the picture,
// then fades out, and is worked out from the time alone.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

constexpr double TIME_FADEIN = 2.5; // seconds the fade in takes
constexpr double TIME_FADEOUT = 2.5; // seconds the fade out takes
constexpr double TIME_HOLD = 0.5; // seconds held on black or on the picture

constexpr double TIME_CYCLE = 2 * TIME_HOLD + TIME_FADEIN + TIME_FADEOUT;

void DEMO_Render(RETRO_Time time)
{
	// Calculate level: it rises to 1 over TIME_FADEIN after the hold on black,
	// stays there over the hold on the picture, and falls back to 0 over the
	// last TIME_FADEOUT of the cycle
	double cycle = fmod(time.total, TIME_CYCLE);
	double level = CLAMP01(MIN((cycle - TIME_HOLD) / TIME_FADEIN, (TIME_CYCLE - cycle) / TIME_FADEOUT));

	// Draw picture
	RETRO_Blit(RETRO_ImageData());
	RETRO_Fade(level, RETRO_ImagePalette());
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
}
