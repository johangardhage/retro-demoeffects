//
// Font
//
// Every embedded font on one screen, each line set in its own font and
// naming it: the IBM VGA 8x8 font, the Amiga Topaz font from Kickstart 1.3
// and 3.0, and Minecraft's ascii.png bitmap font. The framebuffer is
// cleared each frame.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

void DEMO_Render(double time, double deltatime)
{
	RETRO_SetFont(RETRO_FONT_VGA_8X8);
	RETRO_PutString("IBM VGA 8X8", 10, 10, 255);

	RETRO_SetFont(RETRO_FONT_TOPAZ13_8X8);
	RETRO_PutString("AMIGA TOPAZ KICKSTART 1.3 8X8", 10, 34, 255);

	RETRO_SetFont(RETRO_FONT_TOPAZ30_8X8);
	RETRO_PutString("AMIGA TOPAZ KICKSTART 3.0 8X8", 10, 58, 255);

	RETRO_SetFont(RETRO_FONT_MINECRAFT_8X8);
	RETRO_PutString("MINECRAFT ASCII 8X8", 10, 82, 255);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(255, RETRO_WHITE);
}
