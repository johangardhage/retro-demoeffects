//
// fade.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define FADEINSTEPS 500
#define FADEOUTSTEPS 500

void DEMO_Render(double deltatime)
{
	static bool fadein = true;
	static int delay = 30;
	static double step = 0;

	RETRO_Blit(RETRO_ImageData());

	if (fadein && !delay) {
		if (RETRO_FadeIn(FADEINSTEPS, step, RETRO_ImagePalette())) {
			fadein = false;
			step = 0;
			delay = 60;
		} else {
			step += deltatime * 200;
		}
	}

	if (!fadein && !delay) {
		if (RETRO_FadeOut(FADEOUTSTEPS, step, RETRO_ImagePalette())) {
			fadein = true;
			step = 0;
			delay = 30;
		} else {
			step += deltatime * 200;
		}
	}

	if (delay) {
		delay--;
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/fade_320x240.pcx");
}
