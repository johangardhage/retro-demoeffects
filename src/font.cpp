//
// Font
//
// The built-in 8×8 bitmap font, glyphs U+0020..U+007F. Glyph row y is a
// byte; a pixel is on when bit (1 << x) is set, so the LSB is the left
// of the character. Four still rows dump A–Z, a–z, 0–9, and a run of
// punctuation. The framebuffer is cleared each frame; this is not a
// scroller.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

void DEMO_Render(double deltatime)
{
	// Draw charset
	char str1[] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
	char str2[] = { "abcdefghijklmnopqrstuvwxyz" };
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
	RETRO_SetColor(255, RETRO_WHITE);
}
