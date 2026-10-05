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
#include "lib/retropalette.h"

#define FLARE_SPEED 0.6 // radians of the Lissajous per second
#define FLARE_MARGIN 40 // inset of the orbit, in pixels, so the glow stays on screen
#define FLARE_CORE 14 // radius of the lamp itself
#define FLARE_GLOW 38 // and of the soft halo around it
#define FLARE_STREAK 18 // peak the anamorphic row adds at the lamp
#define FLARE_STREAK_WIDTH 28 // pixels the streak is still half-bright
#define FLARE_STREAK_ROWS 3 // rows above and below L_y that take the streak
#define HEX_HALF 0.5f
#define HEX_SQRT3_2 0.86602540378f // √3/2

// One reflection of the aperture: where on the axis it sits, how large and
// how bright it is, and whether it takes the iris's six sides.
static const struct Ghost {
	float s;
	float radius;
	int peak;
	bool hex;
} Ghosts[] = {
	{ 0.22f, 10, 90, false },
	{ 0.45f, 16, 55, true },
	{ 0.70f, 8, 70, false },
	{ 1.00f, 22, 40, true },
	{ 1.28f, 11, 50, true },
	{ 1.55f, 7, 80, false },
};

static void AddPixel(int x, int y, int add)
{
	if (!RETRO_OnScreen(x, y) || add <= 0) {
		return;
	}
	RETRO_PutPixel(x, y, CLAMP256(RETRO_GetPixel(x, y) + add));
}

// Radial disc, brightness (1 − r/R)² peak. r² is compared to R² first so
// the sqrt is only taken inside the disc.
static void AddDisc(float cx, float cy, float radius, int peak)
{
	if (radius < 1.0f || peak <= 0) {
		return;
	}

	int ymin = MAX((int)floor(cy - radius), 0);
	int ymax = MIN((int)ceil(cy + radius), RETRO_HEIGHT - 1);
	int xmin = MAX((int)floor(cx - radius), 0);
	int xmax = MIN((int)ceil(cx + radius), RETRO_WIDTH - 1);
	float r2max = radius * radius;

	for (int y = ymin; y <= ymax; y++) {
		float dy = y + 0.5f - cy;
		for (int x = xmin; x <= xmax; x++) {
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
static void AddHex(float cx, float cy, float radius, int peak)
{
	if (radius < 1.0f || peak <= 0) {
		return;
	}

	int ymin = MAX((int)floor(cy - radius), 0);
	int ymax = MIN((int)ceil(cy + radius), RETRO_HEIGHT - 1);
	int xmin = MAX((int)floor(cx - radius), 0);
	int xmax = MIN((int)ceil(cx + radius), RETRO_WIDTH - 1);

	for (int y = ymin; y <= ymax; y++) {
		float py = (y + 0.5f - cy) / radius;
		for (int x = xmin; x <= xmax; x++) {
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

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * FLARE_SPEED, 2 * M_PI);

	float cx = RETRO_WIDTH / 2.0f;
	float cy = RETRO_HEIGHT / 2.0f;
	float lx = cx + (cx - FLARE_MARGIN) * sin(2 * phase);
	float ly = cy + (cy - FLARE_MARGIN) * sin(3 * phase);

	// Lamp
	AddDisc(lx, ly, FLARE_GLOW, 70);
	AddDisc(lx, ly, FLARE_CORE, 220);

	// Anamorphic streak, 1 / (1 + |x − Lx| / w) on a few rows about Ly
	int ymin = MAX((int)floor(ly) - FLARE_STREAK_ROWS, 0);
	int ymax = MIN((int)ceil(ly) + FLARE_STREAK_ROWS, RETRO_HEIGHT - 1);
	for (int y = ymin; y <= ymax; y++) {
		float fy = 1.0f - fabs(y + 0.5f - ly) / (FLARE_STREAK_ROWS + 0.5f);
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float fx = 1.0f / (1.0f + fabs(x + 0.5f - lx) / FLARE_STREAK_WIDTH);
			AddPixel(x, y, FLARE_STREAK * fx * fy * fy);
		}
	}

	// Ghosts on the optical axis
	for (const Ghost &ghost : Ghosts) {
		float gx = cx + ghost.s * (cx - lx);
		float gy = cy + ghost.s * (cy - ly);
		if (ghost.hex) {
			AddHex(gx, gy, ghost.radius, ghost.peak);
		} else {
			AddDisc(gx, gy, ghost.radius, ghost.peak);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, 80, RETRO_BLACK, RETRO_SCARLET);
	RETRO_CreateGradientPalette(80, 160, RETRO_SCARLET, RETRO_AMBER);
	RETRO_CreateGradientPalette(160, RETRO_COLORS, RETRO_AMBER, RETRO_WHITE);
}
