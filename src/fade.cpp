//
// Fade
//
// A still picture whose palette is scaled. The pixels never change; only
// the DAC does:
//
//   C(s) = s · C_loaded          fading in
//   C(s) = (1 − s) · C_loaded    fading out
//
// s runs from 0 to 1 over TIME_FADEIN / TIME_FADEOUT. A three-state
// machine HOLD / FADEIN / FADEOUT holds for TIME_HOLD on black or on
// the picture, then fades the other way.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TIME_FADEIN 2.5 // seconds the fade in takes
#define TIME_FADEOUT 2.5 // seconds the fade out takes
#define TIME_HOLD 0.5 // seconds held on black or on the picture

enum { FADEIN, FADEOUT, HOLD };

void DEMO_Render(RETRO_Time time)
{
	static int state = HOLD;
	static int next = FADEIN;
	static double hold = TIME_HOLD;
	static double step = 0;

	// Draw picture
	RETRO_Blit(RETRO_ImageData());

	// Fade palette
	switch (state) {
	case HOLD:
		hold -= time.delta;
		if (hold <= 0) {
			state = next;
			step = 0;
		}
		break;
	case FADEIN:
		RETRO_Fade(step, RETRO_ImagePalette());
		if (step >= 1) {
			state = HOLD;
			next = FADEOUT;
			hold = TIME_HOLD;
		} else {
			step += time.delta / TIME_FADEIN;
		}
		break;
	case FADEOUT:
		RETRO_Fade(1 - step, RETRO_ImagePalette());
		if (step >= 1) {
			state = HOLD;
			next = FADEIN;
			hold = TIME_HOLD;
		} else {
			step += time.delta / TIME_FADEOUT;
		}
		break;
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
}
