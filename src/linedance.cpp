//
// Linedance
//
// Each scanline is a horizontal bar whose half-width is
//
//   50 + 10 sin(2π (y+phase1)/256) + 15 cos(4π (y+phase2)/256)
//      + 15 sin(4π (y+phase3)/256)
//
// Three travelling waves on a 256-pixel table: phase1 walks +100, phase2 −100,
// phase3 −200 (pixels a second). The profile is their interference, sliding.
// phase1, phase2, phase3 live on 256. The framebuffer is cleared each frame.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define LINE_PERIOD 256
#define LINE_HALF 50
#define LINE_SPEED1 100 // pixels of phase1 per second
#define LINE_SPEED2 100 // pixels of phase2 per second, falling
#define LINE_SPEED3 200 // pixels of phase3 per second, falling

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase1 = 10;
	static double phase2 = -20;
	static double phase3 = -30;

	phase1 = fmod(phase1 + deltatime * LINE_SPEED1, LINE_PERIOD);
	phase2 = fmod(phase2 - deltatime * LINE_SPEED2, LINE_PERIOD);
	phase3 = fmod(phase3 - deltatime * LINE_SPEED3, LINE_PERIOD);
	if (phase2 < 0) phase2 += LINE_PERIOD;
	if (phase3 < 0) phase3 += LINE_PERIOD;

	// Draw bars
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double w = 10 * sin((y + phase1) * 2 * M_PI / LINE_PERIOD)
			+ 15 * cos((y + phase2) * 4 * M_PI / LINE_PERIOD)
			+ 15 * sin((y + phase3) * 4 * M_PI / LINE_PERIOD);

		int x1 = RETRO_WIDTH / 2 - LINE_HALF - w;
		int x2 = RETRO_WIDTH / 2 + LINE_HALF + w;

		RETRO_DrawLine(x1, y, x2, y, 255);
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_WHITE);
}
