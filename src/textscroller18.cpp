//
// Sinus scroller: fine silver/cyan lettering rolls around a sine ribbon,
// flattening into the dark at its crests, between two magenta raster bars.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

// The reference advances about 125 video pixels/sec at roughly 2x scale.
static constexpr double ScrollSpeed = 64.0;
static constexpr double LetterAdvance = 24.0;
static constexpr double WaveLength = 220.0;
static constexpr double WaveAmplitude = 27.0;
static constexpr int UpperBarY = 106;
static constexpr int LowerBarY = 133;
static constexpr double RippleSpeed = 2.5;
static const char ScrollText[] = "RETRO DEMOEFFECTS...   ";
static double LetterOffsets[sizeof(ScrollText) - 1];
static const char *LetterPaths[sizeof(ScrollText) - 1];
static double TextWidth;
static float Ink[2][RETRO_WIDTH * RETRO_HEIGHT];
static const RETRO_Palette Bars[] = {
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
			float shade = (float)(coverage * (a.light + t * (b.light - a.light)));
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
		double x = origin + (x0 + (x1 - x0) * t) * 4.5;
		double v = (y0 + (y1 - y0) * t) / 6.0 - 0.5;
		// A smaller travelling ripple bends the strokes themselves. Keep the
		// broad ribbon shallow; shortening its wavelength only tilts the text.
		double ripple = x * 0.11 - time * RippleSpeed;
		double bend = sin(v * 5.5 + ripple);
		x += 2.0 * bend;
		// Mapping both coordinates onto the cylinder foreshortens the strokes
		// at the extrema instead of just translating an upright bitmap column.
		double centerangle = (x - 80.0) * (2.0 * M_PI / WaveLength);
		double facing = cos(centerangle);
		// A signed tangent projection flips the return face continuously.
		// Unlike adding glyph height to the wave angle, it cannot fold a
		// letter back into itself near a crest.
		double height = v * 22.0 + 1.2 * sin(v * 5.0 - ripple);
		double y = RETRO_HEIGHT / 2.0 + WaveAmplitude * sin(centerangle) + height * facing;
		double light = 0.18 + 0.82 * pow(fabs(facing), 1.25);
		StrokePoint point = { x, y, light, facing < 0.0 };
		if (i > 0) DrawStrokeSegment(previous, point);
		previous = point;
	}
}

void DEMO_Render(double time, double deltatime)
{
	RETRO_Clear(0);
	memset(Ink, 0, sizeof(Ink));

	double period = TextWidth;
	double distance = time * ScrollSpeed;
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
				if (*p == '/') { connected = false; p++; continue; }
				double gx = *p++ - '0', gy = *p++ - '0';
				if (connected) Stroke(x, lastx, lasty, gx, gy, time);
				lastx = gx; lasty = gy; connected = true;
			}
		}
	}

	// Composite once, after rasterization. Each bar has its own visible
	// face, and its own palette ramp from untouched magenta to foreground ink.
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int bar = -1, row = 0;
		if (abs(y - UpperBarY) <= 3) { bar = 0; row = y - UpperBarY + 3; }
		if (abs(y - LowerBarY) <= 3) { bar = 1; row = y - LowerBarY + 3; }
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int pixel = y * RETRO_WIDTH + x;
			if (bar >= 0) {
				int blend = (int)lround(Ink[bar][pixel] * 8.0);
				RETRO_FrameBuffer()[pixel] = blend == 0 ? 128 + row
					: 135 + (bar * 7 + row) * 8 + blend - 1;
			} else {
				int face = Ink[1][pixel] > Ink[0][pixel] ? 1 : 0;
				int shade = (int)lround(Ink[face][pixel] * 63.0);
				RETRO_FrameBuffer()[pixel] = face * 64 + shade;
			}
		}
	}
}

void DEMO_Initialize(void)
{
	TextWidth = 0.0;
	for (int i = 0; ScrollText[i]; i++) {
		LetterOffsets[i] = TextWidth;
		LetterPaths[i] = nullptr;
		for (const Glyph &glyph : Glyphs) {
			if (glyph.character == ScrollText[i]) LetterPaths[i] = glyph.path;
		}
		TextWidth += ScrollText[i] == ' ' ? 16.0 : ScrollText[i] == '.' ? 12.0 : LetterAdvance;
	}
	for (int i = 0; i < 64; i++) {
		int value = i * 255 / 63;
		RETRO_SetColor(i, value, value, value);
		RETRO_SetColor(64 + i, 0, value * 9 / 10, value);
	}
	for (int row = 0; row < 7; row++) {
		RETRO_SetColor(128 + row, Bars[row]);
		for (int face = 0; face < 2; face++) {
			RETRO_Palette ink = RETRO_GetColor(face * 64 + 63);
			for (int blend = 1; blend <= 8; blend++) {
				RETRO_SetColor(135 + (face * 7 + row) * 8 + blend - 1,
					(Bars[row].r * (8 - blend) + ink.r * blend) / 8,
					(Bars[row].g * (8 - blend) + ink.g * blend) / 8,
					(Bars[row].b * (8 - blend) + ink.b * blend) / 8);
			}
		}
	}
}
