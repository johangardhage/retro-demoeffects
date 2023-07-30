//
// fade.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define FADEINSTEPS 500
#define FADEOUTSTEPS 500

void DEMO_Render(double deltatime)
{
	static bool fadein = true;
	static int delay = 30;
	static double step = 0;

	Texture *texture = RETRO_GetTexture();
	RETRO_Blit(texture->image, texture->width * texture->height);

	if (fadein && !delay) {
		if (RETRO_FadeIn(FADEINSTEPS, step, RETRO_GetTexturePalette())) {
			fadein = false;
			step = 0;
			delay = 60;
		} else {
			step += deltatime * 200;
		}
	}

	if (!fadein && !delay) {
		if (RETRO_FadeOut(FADEOUTSTEPS, step, RETRO_GetTexturePalette())) {
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
	RETRO_LoadTexture("assets/fade_320x240.pcx");
	RETRO_SetBlackPalette();
}
