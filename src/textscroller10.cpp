//
// Scroller, wrapped around a twisted column
//
// A line of text scrolls across a twisted square column, one vertical slice
// per screen column, with the font strip (see FONT below) mapped onto the
// facing spans. The four vertices live on the circle (y, z) = RADIUS
// (sin φ, cos φ) at 90° steps (64 units of a 256-angle table). z is toward
// the viewer. φ is
//
//   φ(x, phase) = COLUMN_LEAN cos phase + A(phase) sin(x/4 + 2 phase)
//   A(phase) = COLUMN_TWIST_MIN
//            + (COLUMN_TWIST_MAX − COLUMN_TWIST_MIN) · (1 + cos(phase/5)) / 2
//
// in table units. The twist amplitude breathes over five turns of the table,
// so it does not lock to the lean and still closes with the rest of the
// motion on ColumnPeriod. An edge is drawn when its first screen coordinate
// is less than its second; the two ascending edges are the visible faces and
// the descending edges face away. Each face is centred on the strip's own
// rows at their native size, with a 1/z shade
//
//   c = 63 · COLUMN_DEPTH_RATE / (COLUMN_RADIUS − z + COLUMN_DEPTH_RATE)
//
// added to a letter (COLUMN_TEXT_SHADE) or the background (0). The strip's u
// scrolls with x + 2 phase, a flat pixel rate independent of FONT_SCALE, so
// a larger strip crosses the screen at the same speed and simply takes
// longer to pass in full. ColumnPeriod is the lcm of the 256-table, the
// twist's five-turn breathing, and the strip's own width. The column is
// centred at y = 159.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8
#define FONT_SCALE 2 // strip pixels per font pixel; the strip is larger but scrolls at the same pixel rate

#define COLUMN_RADIUS 40
#define COLUMN_DEPTH_RATE 20
#define COLUMN_LEAN 100 // table units the column leans as a whole
#define COLUMN_TWIST_MAX 80 // greatest twist amplitude, in table units
#define COLUMN_TWIST_MIN 24 // least twist amplitude as it breathes
#define COLUMN_CENTER_Y 159
#define COLUMN_SHADES 64
#define COLUMN_TEXT_SHADE 64 // added to a letter's texel, selecting the second palette ramp
#define COLUMN_SPEED 60 // table units per second

static const char *const ScrollText[] = { "           RETRO DEMOEFFECTS..." };

RETRO_Image *ScrollImage;

//
// One vertical slice of one face, half-open in y. The strip's own rows are
// centred in the span at their native size rather than stretched to fill
// it, so a letter keeps the size it was drawn at no matter how tall the
// twisting face happens to be; rows the strip does not reach stay
// background. The depth shade is interpolated across the whole span.
//
static void DrawSpan(int top, int bottom, int x, int u, float topshade, float bottomshade)
{
	int height = bottom - top;
	if (height <= 0) {
		return;
	}

	int rowoffset = (height - ScrollImage->height) / 2;
	float shade = topshade;
	float shadestep = (bottomshade - topshade) / height;

	for (int y = top; y < bottom; y++) {
		int row = y - top - rowoffset;
		unsigned char texel = (row >= 0 && row < ScrollImage->height) ? ScrollImage->data[row * ScrollImage->width + u] : 0;
		RETRO_PutPixel(x, COLUMN_CENTER_Y + y, shade + (texel != 0 ? COLUMN_TEXT_SHADE : 0));
		shade += shadestep;
	}
}

void DEMO_Render(double time, double deltatime)
{
	// u = x + 2 phase must return to the same strip column after one
	// period, so the period must be a multiple of width / gcd(width, 2);
	// it must also return the angle table (256) and the twist's five-turn
	// breathing (5*256) to their start
	int scrollstep = 2;
	int scrollperiod = ScrollImage->width / GCD(ScrollImage->width, scrollstep);
	double columnperiod = (double)(5 * 256) * scrollperiod / GCD(5 * 256, scrollperiod);

	// Calculate phase
	double phase = fmod(time * COLUMN_SPEED, columnperiod);

	// Constant over the column, so worked out once rather than per slice
	double twist = COLUMN_TWIST_MIN + (COLUMN_TWIST_MAX - COLUMN_TWIST_MIN) * (1 + COS(phase / 5.0)) / 2;

	// Draw column slices
	for (int x = 0; x < RETRO_WIDTH; x++) {
		double angle = COLUMN_LEAN * COS(phase) + twist * SIN(x / 4.0 + phase * 2);
		double sinradius = COLUMN_RADIUS * SIN(angle);
		double cosradius = COLUMN_RADIUS * COS(angle);
		int cornery[4] = {
			(int)lround(sinradius),
			(int)lround(cosradius),
			(int)lround(-sinradius),
			(int)lround(-cosradius),
		};
		double cornerz[4] = { cosradius, -sinradius, -cosradius, sinradius };
		float cornershade[4];
		for (int corner = 0; corner < 4; corner++) {
			cornershade[corner] = (COLUMN_DEPTH_RATE * (COLUMN_SHADES - 1)) / (COLUMN_RADIUS - cornerz[corner] + COLUMN_DEPTH_RATE);
		}

		// Move scroll, at a flat pixel rate regardless of the strip's size
		int u = WRAP(x + phase * scrollstep, ScrollImage->width);

		// Ascending edges face the viewer and descending edges face away,
		// so DrawSpan rejects the latter.
		for (int corner = 0; corner < 4; corner++) {
			int next = (corner + 1) & 3;
			DrawSpan(cornery[corner], cornery[next], x, u, cornershade[corner], cornershade[next]);
		}
	}
}

void DEMO_Initialize(void)
{
	// Generate the scrolling strip, then give it two pink depth ramps: one
	// for the background and one, offset by COLUMN_TEXT_SHADE, for the letters
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]), FONT_SCALE);
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_DEEPPINK);
	RETRO_CreateGradientPalette(64, 128, RETRO_WINE, RETRO_PINK);
}
