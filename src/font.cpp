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
	char str1[] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
	char str2[] = { "abcdefgjijklmnopqrstuvwzyz" };
	char str3[] = { "0123456789" };
	char str4[] = { "!\"#$%&\'()*+,-./:;<=>?@[\\]^_`" };
	RETRO_PutString(str1, 10, 10, 255);
	RETRO_PutString(str2, 10, 20, 255);
	RETRO_PutString(str3, 10, 30, 255);
	RETRO_PutString(str4, 10, 40, 255);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(255, 255, 255, 255);
}
