//
// showpcx.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

void DEMO_Render(double deltatime)
{
	RETRO_Blit(RETRO_ImageData());
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/showpcx_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
}
