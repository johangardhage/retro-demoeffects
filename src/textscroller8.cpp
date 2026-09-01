//
// Scroller, twirling around a bar
//
// A ribbon of the font strip (see FONT below) coils around an imaginary
// bar lying along the screen. The wave runs along x, the screen's width:
//
//   color = strip[row][WRAP(arc / ARC + phase, stripwidth)] · shade(θ)
//   y     = CY + AMP cos(θ)
//   θ     = START + x · RATE
//   depth = sin(θ)
//   sx    = x · WIDTH / SAMPLE_WIDTH
//   arc   = Σ |d(y, sx)/dx|
//
// x is the sample index, not a screen column. SAMPLE_WIDTH is 3 × WIDTH, so
// three samples share a screen column, and each sample draws the strip's
// own height in pixels, spread sideways and centred on sx. Samples run that
// same width, scaled to sample space, past both ends, so a stamp that
// begins off-screen still paints the columns it overlaps. A letter's own
// rows land across the wave rather than along it, which is what lets the
// ribbon twist a letter as it turns instead of just carrying it flat.
//
// The wave makes TURNS whole turns between the left border and the right
// one, so RATE is 2π · TURNS / SAMPLE_WIDTH radians per sample and the
// ribbon meets both borders the same way. START puts those borders at the
// front of the wave, where depth is −1 and the shade is at its brightest,
// so the text slides in at the very edge of the screen instead of fading
// in a good thirty columns short of it.
//
// The strip is walked by arc length, not by sample index: a column per
// sample would crush the letters into the turns and stretch them over the
// crossings. ARC is screen pixels of path per strip column, so at 1.0 a
// glyph spans its own width in pixels of path wherever it sits, and
// SCROLL_SPEED is the text's speed along the bar.
//
// depth is the sine's other component, the one that would be z if the wave
// were a cylinder. It is in [−1, 1], front-facing when negative. Shade is
// MID − AMP · depth, so the chrome brightens and darkens as the bar turns.
// The back (depth > 0) is scaled again by BACK_DIM. The palette is a ramp
// of the atlas greys, so a texel at full shade is the original colour and a
// darker shade is the same chrome, dimmed. A zero texel is transparent. The
// back is drawn first so the front occludes it, and nothing marks where the
// bar itself is: the twirl shows only in how the letters move, foreshorten
// and vanish, never as a drawn surface.
//
// phase lives on stripwidth, in columns per second.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define SCROLL_SPEED 120.0 // strip columns per second
#define ARC_SCALE 1.0 // screen pixels of path per strip column
#define BAR_CY (RETRO_HEIGHT / 2)
#define BAR_AMP 25 // pixels either side of CY the cosine reaches
#define SAMPLE_WIDTH (RETRO_WIDTH * 3)
#define WAVE_TURNS 3 // whole turns between the left border and the right one
#define WAVE_RATE (2 * M_PI * WAVE_TURNS / SAMPLE_WIDTH) // radians per sample
#define WAVE_START (3 * M_PI / 2) // both borders sit at the front of the wave, at full shade
#define SAMPLE_RUN ((double)RETRO_WIDTH / SAMPLE_WIDTH) // screen columns a sample steps sideways
#define SHADE_MID 100
#define SHADE_AMP 80 // shade is in [20, 180] as depth runs through [−1, 1]
#define SHADE_MAX (SHADE_MID + SHADE_AMP) // full shade, so ink · SHADE_MAX / SHADE_MAX is the atlas grey
#define BACK_DIM 0.4 // extra scale on the back

static const char *const ScrollText[] = { "                               RETRO DEMOEFFECTS..." };

RETRO_Image *ScrollImage;

void DEMO_Render(double time, double deltatime)
{
	// Calculate the phase of the text along the strip
	double phase = fmod(time * SCROLL_SPEED, ScrollImage->width);
	int samplepad = ScrollImage->height * SAMPLE_WIDTH / RETRO_WIDTH; // stamps that start off-screen still draw on screen

	// Draw the bar back to front. Pass 0 is the back (depth > 0), pass 1
	// is the front, so a later sample that lands on the same pixel wins
	// only when it is facing the camera
	for (int pass = 0; pass < 2; pass++) {
		bool front = (pass == 1);

		// Arc length of the path so far. Accumulated over every sample, not
		// just the drawn ones, so both passes place the text identically
		double arc = 0;

		for (int x = -samplepad; x < SAMPLE_WIDTH + samplepad; x++) {
			double theta = WAVE_START + x * WAVE_RATE;
			double depth = sin(theta);
			double run = BAR_AMP * WAVE_RATE * depth; // d(AMP cos θ)/dx, up to sign
			arc += sqrt(run * run + SAMPLE_RUN * SAMPLE_RUN);

			bool facing = (depth <= 0);
			if (facing != front) {
				continue;
			}

			int col = WRAP(arc / ARC_SCALE + phase, ScrollImage->width);
			// Negated: swapping which screen axis carries the wave and which
			// carries the sample index mirrors the letters unless one of the
			// two also has its sign flipped, turning that reflection back
			// into a rotation.
			int y = (int)(BAR_CY - BAR_AMP * cos(theta));
			int sx = x * RETRO_WIDTH / SAMPLE_WIDTH;

			// Shade follows the same sine that places the bar, so the
			// chrome turns with the wave. The back is dimmed again
			int shade = (int)(SHADE_MID - SHADE_AMP * depth);
			if (depth > 0) {
				shade = (int)(shade * BACK_DIM);
			}

			for (int i = 0; i < ScrollImage->height; i++) {
				unsigned char ink = ScrollImage->data[i * ScrollImage->width + col];
				if (ink == 0) {
					continue;
				}

				int px = sx + i - ScrollImage->height / 2;
				int py = y;
				if (px < 0 || px >= RETRO_WIDTH || py < 0 || py >= RETRO_HEIGHT) {
					continue;
				}
				RETRO_PutPixel(px, py, CLAMP256(ink * shade / SHADE_MAX));
			}
		}
	}
}

void DEMO_Initialize(void)
{
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));

	// Remember the darkest and lightest greys the atlas actually uses so the
	// shade ramp can be built from them
	int dim = 255, lit = 0;
	for (int i = 0; i < ScrollImage->width * ScrollImage->height; i++) {
		unsigned char ink = ScrollImage->data[i];
		if (ink != 0) {
			dim = MIN(dim, (int)ink);
			lit = MAX(lit, (int)ink);
		}
	}

	// Init palette. A ramp of the atlas greys: black up to the darkest
	// letter, then up to the lightest, then that chrome for the rest. A
	// texel at full shade is the original colour; a darker shade is the
	// same chrome, dimmed
	RETRO_Palette *fontpal = ScrollImage->palette;
	RETRO_CreateGradientPalette(0, dim, RETRO_BLACK, fontpal[dim]);
	RETRO_CreateGradientPalette(dim, lit + 1, fontpal[dim], fontpal[lit]);
	RETRO_CreateGradientPalette(lit + 1, RETRO_COLORS, fontpal[lit], fontpal[lit]);
}
