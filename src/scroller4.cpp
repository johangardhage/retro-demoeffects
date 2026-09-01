//
// Scroller, a text hurricane
//
// The same 16×16 strip as scroller.cpp, sampled along a vertical sine and
// plotted a column at a time:
//
//   color = strip[row][WRAP(arc / ARC + phase, strip_width)] · shade(θ)
//   x     = CX + AMP cos(θ)
//   θ     = START + y · RATE
//   depth = sin(θ)
//   sy    = y · HEIGHT / SAMPLE_HEIGHT
//   arc   = Σ |d(x, sy)/dy|
//
// y is the sample index, not a screen row. SAMPLE_HEIGHT is 3 × HEIGHT, so
// three samples share a screen row, and each sample draws FONT_HEIGHT
// pixels centred on sy. Samples run SAMPLE_PAD past both ends so a stamp
// that begins off-screen still paints the rows it overlaps. A letter
// stands with its width along the wave and its height across it, and it
// is never bent sideways, only carried along the cosine.
//
// The wave makes TURNS whole turns between the top border and the bottom
// one, so RATE is 2π · TURNS / SAMPLE_HEIGHT radians per sample and the
// ribbon meets both borders the same way. START puts those borders at the
// front of the wave, where depth is −1 and the shade is at its brightest,
// so the text slides in at the very edge of the screen instead of fading
// up out of the dark a good thirty rows short of it.
//
// The strip is walked by arc length, not by sample index. A sample moves
// sqrt((AMP · RATE · sin θ)² + (HEIGHT / SAMPLE_HEIGHT)²) pixels along the
// path, a third of a pixel where the wave turns and close to a whole one
// where it crosses. A column per sample would crush the letters into the
// turns and stretch them over the crossings, and slide the text at a pace
// that visibly surges; against the running total a letter keeps its size
// and its speed all the way round. ARC is screen pixels of path per strip
// column, so at 1.0 a glyph spans FONT_WIDTH pixels of path wherever it
// sits, and SCROLL_SPEED · ARC is the text's speed along the ribbon.
//
// depth is the sine's other component, the one that would be z if the
// wave were a cylinder. It is in [−1, 1], front-facing when negative.
// Shade is MID − AMP · depth, in [20, 180], so the chrome brightens and
// darkens as the ribbon turns. The back (depth > 0) is scaled again by
// BACK_DIM. The palette is a ramp of the atlas greys, so a texel at full
// shade is the original colour and a darker shade is the same chrome,
// dimmed. A zero texel is transparent. The back is drawn first so the
// front occludes it. The plot is clipped against the screen rather than
// the offset being trusted, because RETRO_PutPixel does not clip: it
// asserts in a debug build and writes out of bounds in any other.
//
// phase lives on strip_width, in columns per second.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define IMAGE_WIDTH 944 // atlas pitch; 59 glyphs × 16
#define SCROLL_TEXT "RETRO DEMOEFFECTS   "
#define SCROLL_LENGTH (sizeof(SCROLL_TEXT) - 1)
#define SCROLL_WIDTH (FONT_WIDTH * SCROLL_LENGTH)
#define SCROLL_SPEED 120.0 // strip columns per second
#define ARC_SCALE 1.0 // screen pixels of path per strip column
#define RIBBON_CX (RETRO_WIDTH / 2)
#define RIBBON_AMP 50 // pixels either side of CX the cosine reaches
#define SAMPLE_HEIGHT (RETRO_HEIGHT * 3)
#define WAVE_TURNS 2 // whole turns between the top border and the bottom one
#define WAVE_RATE (2 * M_PI * WAVE_TURNS / SAMPLE_HEIGHT) // radians per sample
#define WAVE_START (3 * M_PI / 2) // both borders sit at the front of the wave, at full shade
#define SAMPLE_RISE ((double)RETRO_HEIGHT / SAMPLE_HEIGHT) // screen rows a sample steps down
#define SAMPLE_PAD (FONT_HEIGHT * SAMPLE_HEIGHT / RETRO_HEIGHT) // stamps that start off-screen still draw on screen
#define SHADE_MID 100
#define SHADE_AMP 80 // shade is in [20, 180] as depth runs through [−1, 1]
#define SHADE_MAX (SHADE_MID + SHADE_AMP) // full shade, so ink · SHADE_MAX / SHADE_MAX is the atlas grey
#define BACK_DIM 0.4 // extra scale on the back

unsigned char strip[FONT_HEIGHT * SCROLL_WIDTH];

void DEMO_Render(double deltatime)
{
	// Calculate the phase of the text along the strip
	static double phase = 0;
	phase = fmod(phase + deltatime * SCROLL_SPEED, SCROLL_WIDTH);

	// Draw the ribbon back to front. Pass 0 is the back (depth > 0), pass 1
	// is the front, so a later sample that lands on the same pixel wins
	// only when it is facing the camera
	for (int pass = 0; pass < 2; pass++) {
		bool front = (pass == 1);

		// Arc length of the path so far. Accumulated over every sample, not
		// just the drawn ones, so both passes place the text identically
		double arc = 0;

		for (int y = -SAMPLE_PAD; y < SAMPLE_HEIGHT + SAMPLE_PAD; y++) {
			double theta = WAVE_START + y * WAVE_RATE;
			double depth = sin(theta);
			double run = RIBBON_AMP * WAVE_RATE * depth; // d(AMP cos θ)/dy, up to sign
			arc += sqrt(run * run + SAMPLE_RISE * SAMPLE_RISE);

			bool facing = (depth <= 0);
			if (facing != front) {
				continue;
			}

			int col = WRAP(arc / ARC_SCALE + phase, SCROLL_WIDTH);
			int x = (int)(RIBBON_CX + RIBBON_AMP * cos(theta));
			int sy = y * RETRO_HEIGHT / SAMPLE_HEIGHT;

			// Shade follows the same sine that places the ribbon, so the
			// chrome turns with the wave. The back is dimmed again
			int shade = (int)(SHADE_MID - SHADE_AMP * depth);
			if (depth > 0) {
				shade = (int)(shade * BACK_DIM);
			}

			for (int i = 0; i < FONT_HEIGHT; i++) {
				unsigned char ink = strip[i * SCROLL_WIDTH + col];
				if (ink == 0) {
					continue;
				}

				int px = x;
				int py = sy + i - FONT_HEIGHT / 2;
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
	RETRO_LoadImage("assets/font_16x16.pcx", true);

	// Init scroll bitmap, and remember the darkest and lightest greys the
	// atlas actually uses so the shade ramp can be built from them
	unsigned char *image = RETRO_ImageData();
	int dim = 255, lit = 0;

	for (int i = 0; i < (int)SCROLL_LENGTH; i++) {
		unsigned char *src = image + ((SCROLL_TEXT[i] - 32) * FONT_WIDTH);
		unsigned char *dst = strip + (i * FONT_WIDTH);

		for (int y = 0; y < FONT_HEIGHT; y++) {
			for (int x = 0; x < FONT_WIDTH; x++) {
				unsigned char ink = src[IMAGE_WIDTH * y + x];
				dst[SCROLL_WIDTH * y + x] = ink;
				if (ink != 0) {
					dim = MIN(dim, (int)ink);
					lit = MAX(lit, (int)ink);
				}
			}
		}
	}

	// Init palette. A ramp of the atlas greys: black up to the darkest
	// letter, then up to the lightest, then that chrome for the rest. A
	// texel at full shade is the original colour; a darker shade is the
	// same chrome, dimmed
	RETRO_Palette *fontpal = RETRO_ImagePalette();
	RETRO_CreateGradientPalette(0, dim, RETRO_BLACK, fontpal[dim]);
	RETRO_CreateGradientPalette(dim, lit + 1, fontpal[dim], fontpal[lit]);
	RETRO_CreateGradientPalette(lit + 1, RETRO_COLORS, fontpal[lit], fontpal[lit]);
}
