//
// Forest scroller on a tapered ribbon that bends along a tiny heightmap.
//
// Uses the original forest with text generated from FONT on a diagonal path.
// The wide end is shifted right; the ribbon extends below the screen until its
// full width clears the bottom edge. Text enters at the narrow tip and moves
// along the ribbon with a blank gap between laps, accelerating at the wide end.
// Foliage stays in front of the text, selected from the background palette.
// Three interleaved pixel groups refresh in turn for the original's shimmer.
//
// Each screen pixel inverts the ribbon's along-path/cross-ribbon coordinates
// directly (the ribbon's x and y are both affine in along, so this is one
// 2x2 linear solve), rather than walking the ribbon and splatting samples
// onto the screen, so every pixel is visited exactly once.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FOREST_BACKGROUND "assets/forest_320x200.pcx"
#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }

#define CELL_COLS 237 // forestscroller.cpp's grid along the spray, so CELL_XOFFSET is the same lead-in
#define CELL_XOFFSET 104 // lap's lead-in gap, same role as forestscroller.cpp's
#define BAND_CYCLE (Band->width + CELL_XOFFSET) // one lap of the window, band plus its lead-in gap
#define TICKS_PER_WINDOW 2.5 // average ticks per column (20% faster than three ticks)
#define END_ACCELERATION 0.10 // reduce column density near the narrow tip, ramping back to normal by the wide end
#define EXIT_ACCELERATION 0.30 // push column density past normal right at the wide end
#define SHIMMER_PHASES 3
#define FONT_BOOST 128 // nonzero font pixels select the background palette's glow range

#define HEIGHTMAP_POINTS 12 // control points spread along the ribbon's length
#define HEIGHTMAP_AMPLITUDE 5.0 // max sideways bend of the ribbon's spine, in pixels

#define PATH_X0 0 // end of the scroll at bottom-left edge of screen
#define PATH_END_SHIFT_X 75 // move the wide end right in pixels, fading to zero at the tip
#define PATH_Y0 191 // wide-end anchor Y
#define PATH_Y1 20 // narrow tip Y
#define PATH_HEIGHT_PEAK 165 // approximate width at (X0,Y0), the end of the scroll
#define PATH_HEIGHT_MIN 25 // approximate width at the tip (X1,Y1), the beginning of the scroll
#define PATH_TAPER_CURVE 0.4 // gentle size boost toward the narrow tip; zero gives a linear taper

// Along-path direction and cross-ribbon direction.
#define UX 0.82
#define UY -0.58
// Letter rows follow the original's approximate 20-degree slant.
#define VX 0.93969262
#define VY 0.34202014

unsigned char *Background;
RETRO_Image *Band;

static const char *const ScrollText[] = { "RETRO DEMOEFFECTS..." };

static unsigned char ForegroundMask[RETRO_WIDTH * RETRO_HEIGHT];

// A tiny 1D heightmap the ribbon's spine follows along its length, so it
// bends like it is tracing a small ridgeline instead of a straight edge.
static double HeightMap[HEIGHTMAP_POINTS];

void DEMO_FixedUpdate(double timestep)
{
	static int tick = 0;
	static bool firstlap = true;

	// x and y are both affine in along - PATH_END_SHIFT_X * (1 - along / alongmax)
	// folds into a single along coefficient - so (along, s) recovers from any
	// screen pixel by inverting x = xorigin + dxdalong * along + VX * s,
	// y = PATH_Y0 + UY * along + VY * s.
	static const double alongmax = (PATH_Y0 - PATH_Y1) / (-UY);
	static const double xorigin = PATH_X0 + PATH_END_SHIFT_X;
	static const double dxdalong = UX - PATH_END_SHIFT_X / alongmax;
	static const double det = dxdalong * VY - VX * UY;

	unsigned char *buffer = RETRO_FrameBuffer();
	int phase = tick % SHIMMER_PHASES;

	// Start with the tip on the last blank column before the first letter.
	int window = (int)(tick / TICKS_PER_WINDOW) + CELL_XOFFSET - CELL_COLS;

	for (int py = 0; py < RETRO_HEIGHT; py++) {
		for (int px = 0; px < RETRO_WIDTH; px++) {
			int addr = py * RETRO_WIDTH + px;
			// Leave other groups intact until their turn, including old text pixels.
			if (ForegroundMask[addr] || (px + py) % SHIMMER_PHASES != phase) {
				continue;
			}

			double dx = px - xorigin;
			double dy = py - PATH_Y0;
			double along = (dx * VY - VX * dy) / det;
			double t = along / alongmax;
			if (t > 1) {
				// Past the narrow tip; the ribbon stops there.
				continue;
			}
			double s = (dxdalong * dy - UY * dx) / det;

			// Keep the exit extension linear so its full-width clipping calculation holds.
			double taper = 1 - t;
			if (t > 0) {
				taper += PATH_TAPER_CURVE * t * t * (1 - t);
			}
			double height = PATH_HEIGHT_MIN + (PATH_HEIGHT_PEAK - PATH_HEIGHT_MIN) * taper;

			// Bend the ribbon's spine to follow HeightMap along its length, smoothly
			// interpolated between control points, rather than a dead-straight line.
			double u = CLAMP01(along / alongmax) * (HEIGHTMAP_POINTS - 1);
			int u0 = (int)u;
			int u1 = u0 + 1 < HEIGHTMAP_POINTS ? u0 + 1 : u0;
			double ufrac = u - u0;
			double uweight = ufrac * ufrac * (3 - 2 * ufrac);
			double bend = HeightMap[u0] + (HeightMap[u1] - HeightMap[u0]) * uweight;
			double ssampled = s + bend;
			if (ssampled < -height / 2 || ssampled >= height / 2) {
				continue;
			}

			// Fewer columns per pixel near the tip hold letters back there, and the
			// exit term pushes past-normal density right at the wide end, so text
			// visibly speeds up approaching the exit. Both terms vanish at t=0 and
			// t=1, so they only reshape the middle - textt(0) and textt(1) (and
			// so the column range covered end to end) are unchanged.
			double textt = t + END_ACCELERATION * (1 - t) * (1 - t) - EXIT_ACCELERATION * t * (1 - t);
			int col = (int)lround(textt * (CELL_COLS - 1));
			// Negative columns carry the lettering past the wide-end anchor.
			int strip = WRAP(col + window, BAND_CYCLE);
			// At startup there is no previous lap trailing through the extension.
			bool waitingfortext = firstlap && col + window < 0;

			int row = (int)((ssampled + height / 2) * Band->height / height);
			row = CLAMP(row, 0, Band->height);
			unsigned char fontvalue = 0;
			if (!waitingfortext && strip >= CELL_XOFFSET) {
				unsigned char shading = Band->data[row * Band->width + (strip - CELL_XOFFSET)];
				if (shading) {
					// The font atlas' antialiasing levels run 0-255; bring them down to
					// a 0-7 nudge so they shade into the glow range instead of past it.
					fontvalue = FONT_BOOST + (shading >> 5);
				}
			}

			buffer[addr] = Background[addr] + fontvalue;
		}
	}

	tick = WRAP(tick + 1, (int)(BAND_CYCLE * TICKS_PER_WINDOW));
	if (tick == 0) {
		firstlap = false;
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *background = RETRO_LoadImage(FOREST_BACKGROUND, true);
	Background = background->data;
	RETRO_Blit(Background);

	Band = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]), 2);

	for (int addr = 0; addr < RETRO_WIDTH * RETRO_HEIGHT; addr++) {
		RETRO_Palette color = background->palette[Background[addr]];
		if (color.g >= color.b) {
			ForegroundMask[addr] = 1;
		}
	}

	for (int i = 0; i < HEIGHTMAP_POINTS; i++) {
		HeightMap[i] = (RAND() * 2 - 1) * HEIGHTMAP_AMPLITUDE;
	}
}
