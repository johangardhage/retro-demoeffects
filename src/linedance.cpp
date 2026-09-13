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
// A bar's color follows the same w that sizes it: constructive interference
// (w > 0, the bar swollen) runs white through magenta, destructive (w < 0,
// the bar pinched) runs white through blue, so the palette makes the
// interference visible instead of just the outline.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define LINE_PERIOD 256
#define LINE_HALF 50
#define LINE_SPEED1 100 // pixels of phase1 per second
#define LINE_SPEED2 100 // pixels of phase2 per second, falling
#define LINE_SPEED3 200 // pixels of phase3 per second, falling

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase1 = fmod(10.0 + time * LINE_SPEED1, LINE_PERIOD);
	double phase2 = fmod(-20.0 - time * LINE_SPEED2, LINE_PERIOD);
	double phase3 = fmod(-30.0 - time * LINE_SPEED3, LINE_PERIOD);
	if (phase2 < 0) phase2 += LINE_PERIOD;
	if (phase3 < 0) phase3 += LINE_PERIOD;

	// Draw bars
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double w = 10 * sin((y + phase1) * 2 * M_PI / LINE_PERIOD)
			+ 15 * cos((y + phase2) * 4 * M_PI / LINE_PERIOD)
			+ 15 * sin((y + phase3) * 4 * M_PI / LINE_PERIOD);

		int x1 = RETRO_WIDTH / 2.0 - LINE_HALF - w;
		int x2 = RETRO_WIDTH / 2.0 + LINE_HALF + w;

		// w's max amplitude is 10 + 15 + 15 = 40, so scale it across the
		// two half-palettes centered on the neutral, white midpoint
		unsigned char color = 128 + CLAMP(w * 127 / 40, -127, 128);

		RETRO_DrawLine(x1, y, x2, y, color);
	}
}

void DEMO_Initialize(void)
{
	// Index 0 stays black for the cleared background. Neutral white sits at
	// the midpoint, cooling to blue below it and warming to magenta above it
	// as the bars pinch or swell
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, 128, RETRO_BLUE, RETRO_WHITE);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_WHITE, RETRO_MAGENTA);
}
