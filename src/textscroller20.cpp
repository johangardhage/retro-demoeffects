//
// Sanity WOC92 sine scroller with a continuous sinusoidal zoom.
// Inspired by scroller3.mp4; only the lettering is rendered, on black.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"

static constexpr int GlyphWidth = 256;
static constexpr int GlyphHeight = 128;
static constexpr double LetterWidth = 208.0;
static constexpr double InkWidth = 200.0;
static constexpr double ScrollSpeed = 300.0;
static constexpr double BounceStart = 5.0;
static constexpr double BouncePeriod = 1.8;
static constexpr double SineStart = BounceStart + BouncePeriod;
static const char ScrollText[] = "RETRO DEMOEFFECTS...   ";
// Cruiser's blue font from Sanity's World of Commodore 1992 demo.
// PCX cells follow ASCII from space, with black at index 0 and blue at 1.
static RETRO_Font Font;

static double Sample(unsigned char character, double u, double v)
{
	int glyph = character - Font.firstcharacter;
	if (glyph < 0 || (glyph + 1) * GlyphWidth > Font.atlas->width
		|| u < 0.0 || v < 0.0 || u >= GlyphWidth || v >= GlyphHeight) return 0.0;
	int x = (int)u, y = (int)v;
	int nextx = MIN(x + 1, GlyphWidth - 1), nexty = MIN(y + 1, GlyphHeight - 1);
	double fx = u - x, fy = v - y;
	const unsigned char *top = Font.atlas->data + y * Font.atlas->width + glyph * GlyphWidth;
	const unsigned char *bottom = Font.atlas->data + nexty * Font.atlas->width + glyph * GlyphWidth;
	return 255.0 * (((1.0 - fx) * top[x] + fx * top[nextx]) * (1.0 - fy)
		+ ((1.0 - fx) * bottom[x] + fx * bottom[nextx]) * fy);
}

static double Ease(double t)
{
	t = CLAMP01(t);
	return t * t * (3.0 - 2.0 * t);
}

void DEMO_Render(double time, double deltatime)
{
	RETRO_Clear(0);
	constexpr int TextLength = sizeof(ScrollText) - 1;
	double distance = time * ScrollSpeed;
	double phase = fmod(distance, TextLength * LetterWidth);
	// After five seconds, smoothly repeat a deeper 1.8-second sinusoidal zoom.
	// Sine scrolling joins after one bounce.
	double bounce = MAX(0.0, time - BounceStart);
	// Start at the crest so both scale and velocity join the plain intro
	// continuously. Cosine is a sine wave shifted by a quarter turn.
	double zoom = 0.60 + 0.40 * cos(bounce * (2.0 * M_PI / BouncePeriod));
	double sinetime = MAX(0.0, time - SineStart);
	double sineamplitude = 26.0 * Ease(sinetime);
	double height = 100.0;
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double py = (y - RETRO_HEIGHT / 2.0) / zoom;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double sx = (x - RETRO_WIDTH / 2.0) / zoom + RETRO_WIDTH / 2.0;
			double wave = sx * 0.023 - sinetime * 2.4;
			double center = sineamplitude * sin(wave);
			double v = ((py - center) / height + 0.5) * (GlyphHeight - 1);
			// Begin offscreen at the right; subsequent passes wrap continuously.
			if (sx + distance < RETRO_WIDTH) continue;
			double position = sx + phase - RETRO_WIDTH;
			int letter = (int)floor(position / LetterWidth);
			double u = (position - letter * LetterWidth - (LetterWidth - InkWidth) / 2.0) * GlyphWidth / InkWidth;
			unsigned char character = ScrollText[WRAP(letter, TextLength)];
			RETRO_PutPixel(x, y, (unsigned char)lround(Sample(character, u, v)));
		}
	}
}

void DEMO_Initialize(void)
{
	Font = RETRO_LoadFont(RETRO_FontAsset{ "assets/font_256x128.pcx", GlyphWidth, GlyphHeight });
	// The original #7777ff blue, with a coverage ramp for smooth zooming.
	RETRO_Palette blue = Font.atlas->palette[1];
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i * blue.r / 255, i * blue.g / 255, i * blue.b / 255);
	}
}
