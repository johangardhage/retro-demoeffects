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
#include "lib/retrogfx.h"

#define TIME_FADEIN 2.5 // seconds the fade in takes
#define TIME_FADEOUT 2.5 // seconds the fade out takes
#define TIME_HOLD 0.5 // seconds held on black or on the picture

enum { FADEIN, FADEOUT, HOLD };

void DEMO_Render(double time, double deltatime)
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
		hold -= deltatime;
		if (hold <= 0) {
			state = next;
			step = 0;
		}
		break;
	case FADEIN:
		if (RETRO_FadeIn(RETRO_COLORS, step * RETRO_COLORS, RETRO_ImagePalette())) {
			state = HOLD;
			next = FADEOUT;
			hold = TIME_HOLD;
		} else {
			step += deltatime / TIME_FADEIN;
		}
		break;
	case FADEOUT:
		if (RETRO_FadeOut(RETRO_COLORS, step * RETRO_COLORS, RETRO_ImagePalette())) {
			state = HOLD;
			next = FADEIN;
			hold = TIME_HOLD;
		} else {
			step += deltatime / TIME_FADEOUT;
		}
		break;
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
}
