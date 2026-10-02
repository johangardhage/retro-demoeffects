//
// Scroller, on a sine with a zoom
//
// A sine scroller with a continuous sinusoidal zoom. Only the lettering is
// rendered, on black.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"
#include "lib/retrovector.h"

#define GLYPH_WIDTH 256
#define GLYPH_HEIGHT 128
#define LETTER_WIDTH 208.0
#define INK_WIDTH 200.0
#define SCROLL_SPEED 300.0
#define BOUNCE_START 5.0
#define BOUNCE_PERIOD 1.8
#define SINE_START (BOUNCE_START + BOUNCE_PERIOD)

static const char ScrollText[] = "RETRO DEMOEFFECTS...   ";
// Cruiser's blue font from Sanity's World of Commodore 1992 demo.
// PCX cells follow ASCII from space, with black at index 0 and blue at 1.
static RETRO_Font Font;

static double Sample(unsigned char character, double u, double v)
{
	int glyph = character - Font.firstcharacter;
	if (glyph < 0 || (glyph + 1) * GLYPH_WIDTH > Font.atlas->width
		|| u < 0.0 || v < 0.0 || u >= GLYPH_WIDTH || v >= GLYPH_HEIGHT) return 0.0;
	int x = (int)u, y = (int)v;
	int nextx = MIN(x + 1, GLYPH_WIDTH - 1), nexty = MIN(y + 1, GLYPH_HEIGHT - 1);
	double fx = u - x, fy = v - y;
	const unsigned char *top = Font.atlas->data + y * Font.atlas->width + glyph * GLYPH_WIDTH;
	const unsigned char *bottom = Font.atlas->data + nexty * Font.atlas->width + glyph * GLYPH_WIDTH;
	return 255.0 * mix(mix(top[x], top[nextx], fx), mix(bottom[x], bottom[nextx], fx), fy);
}

void DEMO_Render(double time, double deltatime)
{
	int textlength = sizeof(ScrollText) - 1;
	double distance = time * SCROLL_SPEED;
	double phase = fmod(distance, textlength * LETTER_WIDTH);
	// After five seconds, smoothly repeat a deeper 1.8-second sinusoidal zoom.
	// Sine scrolling joins after one bounce.
	double bounce = MAX(0.0, time - BOUNCE_START);
	// Start at the crest so both scale and velocity join the plain intro
	// continuously. Cosine is a sine wave shifted by a quarter turn.
	double zoom = 0.60 + 0.40 * cos(bounce * (2.0 * M_PI / BOUNCE_PERIOD));
	double sinetime = MAX(0.0, time - SINE_START);
	double sineamplitude = 26.0 * smoothstep(0.0, 1.0, sinetime);
	double height = 100.0;
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double py = (y - RETRO_HEIGHT / 2.0) / zoom;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double sx = (x - RETRO_WIDTH / 2.0) / zoom + RETRO_WIDTH / 2.0;
			double wave = sx * 0.023 - sinetime * 2.4;
			double center = sineamplitude * sin(wave);
			double v = ((py - center) / height + 0.5) * (GLYPH_HEIGHT - 1);
			// Begin offscreen at the right; subsequent passes wrap continuously.
			if (sx + distance < RETRO_WIDTH) continue;
			double position = sx + phase - RETRO_WIDTH;
			int letter = (int)floor(position / LETTER_WIDTH);
			double u = (position - letter * LETTER_WIDTH - (LETTER_WIDTH - INK_WIDTH) / 2.0) * GLYPH_WIDTH / INK_WIDTH;
			unsigned char character = ScrollText[WRAP(letter, textlength)];
			RETRO_PutPixel(x, y, (unsigned char)lround(Sample(character, u, v)));
		}
	}
}

void DEMO_Initialize(void)
{
	Font = RETRO_LoadFont(RETRO_FontAsset{ "assets/font_256x128.pcx", GLYPH_WIDTH, GLYPH_HEIGHT });
	// The original #7777ff blue, with a coverage ramp for smooth zooming.
	RETRO_Palette blue = Font.atlas->palette[1];
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i * blue.r / 255, i * blue.g / 255, i * blue.b / 255);
	}
}
