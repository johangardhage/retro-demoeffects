//
// font.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

void DEMO_Render(double deltatime)
{
	char str[] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZ!?" };
	RETRO_PutString(str, 10, 10, 255);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(255, 255, 255, 255);
}
