//
// Scroller, rolling around a sine ribbon
//
// Fine silver/cyan lettering rolls around a sine ribbon, flattening into the
// dark at its crests, between two magenta raster bars.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrovector.h"

// The reference advances about 125 video pixels/sec at roughly 2x scale.
#define SCROLL_SPEED 64.0
#define LETTER_ADVANCE 24.0
#define WAVE_LENGTH 220.0
#define WAVE_AMPLITUDE 27.0
#define UPPER_BAR_Y 106
#define LOWER_BAR_Y 133
#define RIPPLE_SPEED 2.5

// The palette: a silver ink ramp and a cyan one, the rows of a raster bar,
// then for each face and bar row a ramp from the bar's color to full ink.
#define INK_SHADES 64 // shades in each ink ramp
#define BAR_ROWS 7 // scanlines a raster bar is tall
#define BAR_BASE (2 * INK_SHADES)
#define BLEND_STEPS 8 // steps from a bar row's own color to the ink over it
#define BLEND_BASE (BAR_BASE + BAR_ROWS)

static const char ScrollText[] = "RETRO DEMOEFFECTS...   ";
static double LetterOffsets[sizeof(ScrollText) - 1];
static const char *LetterPaths[sizeof(ScrollText) - 1];
static double TextWidth;
static float Ink[2][RETRO_WIDTH * RETRO_HEIGHT];
static const RETRO_Palette Bars[BAR_ROWS] = {
	{ 44, 0, 48 }, { 115, 0, 126 }, { 202, 0, 214 },
	{ 255, 24, 255 }, { 219, 0, 230 }, { 137, 0, 147 }, { 52, 0, 59 }
};

// Single-stroke outlines, in a 4 by 6 design grid. A slash lifts the pen.
// Curves are chamfered so the small letters keep the angular demo font look.
struct Glyph { char character; const char *path; };
static const Glyph Glyphs[] = {
	{ 'R', "06003041423303/2346" },
	{ 'E', "40000646/0323" },
	{ 'T', "0040/2026" },
	{ 'O', "103041453616050110" },
	{ 'D', "00063645413000" },
	{ 'M', "0600224046" },
	{ 'F', "060040/0323" },
	{ 'C', "4130100105163645" },
	{ 'S', "413010010213334445361605" },
	{ '.', "2526" },
};

struct StrokePoint { double x, y, light; bool cyan; };

// Rasterize short projected segments by distance to the stroke, avoiding
// brightness changes caused by the sampling points landing between pixels.
static void DrawStrokeSegment(const StrokePoint &a, const StrokePoint &b)
{
	double dx = b.x - a.x, dy = b.y - a.y;
	double length2 = dx * dx + dy * dy;
	int left = MAX(0, (int)floor(MIN(a.x, b.x) - 1.0));
	int right = MIN(RETRO_WIDTH - 1, (int)ceil(MAX(a.x, b.x) + 1.0));
	int top = MAX(0, (int)floor(MIN(a.y, b.y) - 1.0));
	int bottom = MIN(RETRO_HEIGHT - 1, (int)ceil(MAX(a.y, b.y) + 1.0));
	for (int y = top; y <= bottom; y++) {
		for (int x = left; x <= right; x++) {
			double t = length2 > 0.0 ? CLAMP01(((x - a.x) * dx + (y - a.y) * dy) / length2) : 0.0;
			double distance = hypot(x - a.x - t * dx, y - a.y - t * dy);
			double coverage = MAX(0.0, 1.0 - distance);
			float shade = (float)(coverage * mix(a.light, b.light, t));
			float &ink = Ink[t < 0.5 ? a.cyan : b.cyan][y * RETRO_WIDTH + x];
			ink = MAX(ink, shade);
		}
	}
}

static void Stroke(double origin, double x0, double y0, double x1, double y1, double time)
{
	double length = hypot((x1 - x0) * 4.0, (y1 - y0) * 4.0);
	int steps = MAX(1, (int)ceil(length * 2.0));
	StrokePoint previous = {};
	for (int i = 0; i <= steps; i++) {
		double t = (double)i / steps;
		double x = origin + mix(x0, x1, t) * 4.5;
		double v = mix(y0, y1, t) / 6.0 - 0.5;
		// A smaller traveling ripple bends the strokes themselves. Keep the
		// broad ribbon shallow; shortening its wavelength only tilts the text.
		double ripple = x * 0.11 - time * RIPPLE_SPEED;
		double bend = sin(v * 5.5 + ripple);
		x += 2.0 * bend;
		// Mapping both coordinates onto the cylinder foreshortens the strokes
		// at the extrema instead of just translating an upright bitmap column.
		double centerangle = (x - 80.0) * (2.0 * M_PI / WAVE_LENGTH);
		double facing = cos(centerangle);
		// A signed tangent projection flips the return face continuously.
		// Unlike adding glyph height to the wave angle, it cannot fold a
		// letter back into itself near a crest.
		double height = v * 22.0 + 1.2 * sin(v * 5.0 - ripple);
		double y = RETRO_HEIGHT / 2.0 + WAVE_AMPLITUDE * sin(centerangle) + height * facing;
		double light = 0.18 + 0.82 * pow(fabs(facing), 1.25);
		StrokePoint point = { x, y, light, facing < 0.0 };
		if (i > 0) DrawStrokeSegment(previous, point);
		previous = point;
	}
}

void DEMO_Render(RETRO_Time time)
{
	memset(Ink, 0, sizeof(Ink));

	double period = TextWidth;
	double distance = time.total * SCROLL_SPEED;
	double phase = fmod(distance, period);
	for (int repeat = -1; repeat <= 0; repeat++) {
		// There is no preceding copy on the first pass: start with an empty
		// screen and let the first letter arrive from the right edge.
		if (repeat < 0 && distance < period) continue;
		for (int i = 0; ScrollText[i]; i++) {
			double x = RETRO_WIDTH + 3.0 + LetterOffsets[i] - phase + repeat * period;
			if (!LetterPaths[i] || x < -21.0 || x >= RETRO_WIDTH + 2.0) continue;
			bool connected = false;
			double lastx = 0, lasty = 0;
			for (const char *p = LetterPaths[i]; *p;) {
				if (*p == '/') {
					connected = false;
					p++;
					continue;
				}
				double gx = *p++ - '0', gy = *p++ - '0';
				if (connected) Stroke(x, lastx, lasty, gx, gy, time.total);
				lastx = gx;
				lasty = gy;
				connected = true;
			}
		}
	}

	// Composite once, after rasterization. Each bar has its own visible
	// face, and its own palette ramp from untouched magenta to foreground ink.
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int bar = -1, row = 0;
		if (abs(y - UPPER_BAR_Y) <= BAR_ROWS / 2) {
			bar = 0;
			row = y - UPPER_BAR_Y + BAR_ROWS / 2;
		}
		if (abs(y - LOWER_BAR_Y) <= BAR_ROWS / 2) {
			bar = 1;
			row = y - LOWER_BAR_Y + BAR_ROWS / 2;
		}
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int pixel = y * RETRO_WIDTH + x;
			if (bar >= 0) {
				int blend = (int)lround(Ink[bar][pixel] * BLEND_STEPS);
				RETRO_FrameBuffer()[pixel] = blend == 0 ? BAR_BASE + row
					: BLEND_BASE + (bar * BAR_ROWS + row) * BLEND_STEPS + blend - 1;
			} else {
				int face = Ink[1][pixel] > Ink[0][pixel] ? 1 : 0;
				int shade = (int)lround(Ink[face][pixel] * (INK_SHADES - 1));
				RETRO_FrameBuffer()[pixel] = face * INK_SHADES + shade;
			}
		}
	}
}

void DEMO_Initialize(void)
{
	TextWidth = 0.0;
	for (int i = 0; ScrollText[i]; i++) {
		LetterOffsets[i] = TextWidth;
		LetterPaths[i] = NULL;
		for (const Glyph &glyph : Glyphs) {
			if (glyph.character == ScrollText[i]) LetterPaths[i] = glyph.path;
		}
		TextWidth += ScrollText[i] == ' ' ? 16.0 : ScrollText[i] == '.' ? 12.0 : LETTER_ADVANCE;
	}
	for (int i = 0; i < INK_SHADES; i++) {
		int value = i * 255 / (INK_SHADES - 1);
		RETRO_SetColor(i, value, value, value);
		RETRO_SetColor(INK_SHADES + i, 0, value * 9 / 10, value);
	}
	for (int row = 0; row < BAR_ROWS; row++) {
		RETRO_SetColor(BAR_BASE + row, Bars[row]);
		for (int face = 0; face < 2; face++) {
			RETRO_Palette ink = RETRO_GetColor(face * INK_SHADES + INK_SHADES - 1);
			for (int blend = 1; blend <= BLEND_STEPS; blend++) {
				RETRO_SetColor(BLEND_BASE + (face * BAR_ROWS + row) * BLEND_STEPS + blend - 1,
					(Bars[row].r * (BLEND_STEPS - blend) + ink.r * blend) / BLEND_STEPS,
					(Bars[row].g * (BLEND_STEPS - blend) + ink.g * blend) / BLEND_STEPS,
					(Bars[row].b * (BLEND_STEPS - blend) + ink.b * blend) / BLEND_STEPS);
			}
		}
	}
}
