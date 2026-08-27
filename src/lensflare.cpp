//
// Lens flare
//
// A point light and the ghosts an iris throws of it. The light sits at L.
// The optical axis is the principal point C = (W/2, H/2). Every reflection
// of the aperture lives on the line through both:
//
//   G(s) = C + s (C − L)
//
// s = 0 is C itself, s = 1 the antipode at equal distance the other side of
// C, and s = −1 the light. Every ghost here is given a positive s, so they
// all sit on the far side of the axis from the lamp, which is where a real
// iris throws them. The ghosts are discs and hexagons of that family. A
// hexagon is the cube-coordinate distance
//
//   d = max(|x|, |x/2 + y √3/2|, |−x/2 + y √3/2|)
//
// which is 1 on the six-sided iris. Brightness is additive in palette
// index, (1 − d)² peak, clamped at 255; the palette is a heat ramp, so
// adding is lighting. The anamorphic streak is a few rows about L_y, the
// way a cylindrical element streaks a lamp, and it does not use that
// falloff: (1 − d)² reaches zero at the edge of a ghost, where a streak has
// to run to the edge of the screen and still be there. Its falloff in x is
// 1 / (1 + |x − L_x| / w), which has no zero at all and only ever halves.
// L rides a 2:3 Lissajous inset so the glow stays on screen. Phase lives
// on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define FLARE_SPEED 0.6 // radians of the Lissajous per second
#define FLARE_MARGIN 40 // inset of the orbit, in pixels, so the glow stays on screen
#define FLARE_CORE 14 // radius of the lamp itself
#define FLARE_GLOW 38 // and of the soft halo around it
#define FLARE_STREAK 18 // peak the anamorphic row adds at the lamp
#define FLARE_STREAK_WIDTH 28 // pixels the streak is still half-bright
#define FLARE_STREAK_ROWS 3 // rows above and below L_y that take the streak
#define HEX_HALF 0.5f
#define HEX_SQRT3_2 0.86602540378f // √3/2

static const float GhostS[] = { 0.22f, 0.45f, 0.70f, 1.00f, 1.28f, 1.55f };
static const float GhostR[] = { 10, 16, 8, 22, 11, 7 };
static const float GhostPeak[] = { 90, 55, 70, 40, 50, 80 };
static const bool GhostHex[] = { false, true, false, true, true, false };
#define NUM_GHOSTS 6

void AddPixel(int x, int y, int add)
{
	if (x < 0 || x >= RETRO_WIDTH || y < 0 || y >= RETRO_HEIGHT || add <= 0) {
		return;
	}
	int color = RETRO_GetPixel(x, y) + add;
	RETRO_PutPixel(x, y, color > 255 ? 255 : color);
}

// Radial disc, brightness (1 − r/R)² peak. r² is compared to R² first so
// the sqrt is only taken inside the disc.
void AddDisc(float cx, float cy, float radius, int peak)
{
	if (radius < 1.0f || peak <= 0) {
		return;
	}

	int y0 = MAX((int)floor(cy - radius), 0);
	int y1 = MIN((int)ceil(cy + radius), RETRO_HEIGHT - 1);
	int x0 = MAX((int)floor(cx - radius), 0);
	int x1 = MIN((int)ceil(cx + radius), RETRO_WIDTH - 1);
	float r2max = radius * radius;

	for (int y = y0; y <= y1; y++) {
		float dy = y + 0.5f - cy;
		for (int x = x0; x <= x1; x++) {
			float dx = x + 0.5f - cx;
			float r2 = dx * dx + dy * dy;
			if (r2 >= r2max) {
				continue;
			}
			float t = 1.0f - sqrt(r2) / radius;
			AddPixel(x, y, peak * t * t);
		}
	}
}

// Hexagonal iris, same (1 − d)² peak with d the cube-coordinate distance.
void AddHex(float cx, float cy, float radius, int peak)
{
	if (radius < 1.0f || peak <= 0) {
		return;
	}

	int y0 = MAX((int)floor(cy - radius), 0);
	int y1 = MIN((int)ceil(cy + radius), RETRO_HEIGHT - 1);
	int x0 = MAX((int)floor(cx - radius), 0);
	int x1 = MIN((int)ceil(cx + radius), RETRO_WIDTH - 1);

	for (int y = y0; y <= y1; y++) {
		float py = (y + 0.5f - cy) / radius;
		for (int x = x0; x <= x1; x++) {
			float px = (x + 0.5f - cx) / radius;
			float d = fabs(px);
			float d2 = fabs(px * HEX_HALF + py * HEX_SQRT3_2);
			float d3 = fabs(-px * HEX_HALF + py * HEX_SQRT3_2);
			float hex = MAX(d, MAX(d2, d3));
			if (hex >= 1.0f) {
				continue;
			}
			float t = 1.0f - hex;
			AddPixel(x, y, peak * t * t);
		}
	}
}

void DEMO_Render(double deltatime)
{
	static double phase = 0;
	phase = fmod(phase + deltatime * FLARE_SPEED, 2 * M_PI);

	float cx = RETRO_WIDTH / 2.0f;
	float cy = RETRO_HEIGHT / 2.0f;
	float lx = cx + (cx - FLARE_MARGIN) * sin(2 * phase);
	float ly = cy + (cy - FLARE_MARGIN) * sin(3 * phase);

	// Lamp
	AddDisc(lx, ly, FLARE_GLOW, 70);
	AddDisc(lx, ly, FLARE_CORE, 220);

	// Anamorphic streak, 1 / (1 + |x − Lx| / w) on a few rows about Ly
	int y0 = MAX((int)floor(ly) - FLARE_STREAK_ROWS, 0);
	int y1 = MIN((int)ceil(ly) + FLARE_STREAK_ROWS, RETRO_HEIGHT - 1);
	for (int y = y0; y <= y1; y++) {
		float fy = 1.0f - fabs(y + 0.5f - ly) / (FLARE_STREAK_ROWS + 0.5f);
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float fx = 1.0f / (1.0f + fabs(x + 0.5f - lx) / FLARE_STREAK_WIDTH);
			AddPixel(x, y, FLARE_STREAK * fx * fy * fy);
		}
	}

	// Ghosts on the optical axis
	for (int i = 0; i < NUM_GHOSTS; i++) {
		float gx = cx + GhostS[i] * (cx - lx);
		float gy = cy + GhostS[i] * (cy - ly);
		if (GhostHex[i]) {
			AddHex(gx, gy, GhostR[i], GhostPeak[i]);
		} else {
			AddDisc(gx, gy, GhostR[i], GhostPeak[i]);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, 80, RETRO_BLACK, RETRO_SCARLET);
	RETRO_CreateGradientPalette(80, 160, RETRO_SCARLET, RETRO_AMBER);
	RETRO_CreateGradientPalette(160, RETRO_COLORS, RETRO_AMBER, RETRO_WHITE);
}
