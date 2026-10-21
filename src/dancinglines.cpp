//
// Dancing lines
//
// A chain of POINTS attractors. Point 0 is a wandering walker; each later
// point is
//
//   p_i' = (p_i + p_{i-1}) / (2 + k / POINTS)
//
// k = 0 is the midpoint. k/POINTS is a small shrink (k > 0) or swell
// (k < 0) toward the origin. k = 0.75 + sin(phase / 15) ∈ [−0.25, 1.75].
// Segments are drawn in all four mirror quadrants. The framebuffer is never
// cleared; a fire blur after each step is what fades the trails.
//
// The walker is a pair of incommensurate radiuses, rotated by −α
// (α = phase / 1.37). phase lives on 2π · 2 · 3³ · 5 · 13 · 41 · 137,
// the shared period of 1.37, 4.1, 1.3, 2, 2.7, 15 and the α/8 blur.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

constexpr int POINTS = 170;
constexpr double LINE_SPEED = 2.5;
constexpr double LINE_PERIOD = 2 * M_PI * 19715670.0; // 2π · 2 · 3³ · 5 · 13 · 41 · 137

static void DrawLines(float x, float y, float k)
{
	static vec2 points[POINTS];

	points[0] = { x, y };

	for (int i = 1; i < POINTS; i++) {
		points[i] = (points[i] + points[i - 1]) / (float)(2.0 + k / POINTS);

		// Not clamped: clamping x and y apart bends the segment. The line clips.
		// A point sits on a pixel center, half a pixel in from the corner the
		// subpixel line counts from, and the mirror of a coordinate c across
		// the screen is size - c.
		float x1 = points[i].x + 0.5f;
		float x2 = points[i - 1].x + 0.5f;
		float y1 = points[i].y + 0.5f;
		float y2 = points[i - 1].y + 0.5f;

		RETRO_DrawSubpixelLine(x1, y1, x2, y2, 255);
		RETRO_DrawSubpixelLine(x1, RETRO_HEIGHT - y1, x2, RETRO_HEIGHT - y2, 255);
		RETRO_DrawSubpixelLine(RETRO_WIDTH - x1, y1, RETRO_WIDTH - x2, y2, 255);
		RETRO_DrawSubpixelLine(RETRO_WIDTH - x1, RETRO_HEIGHT - y1, RETRO_WIDTH - x2, RETRO_HEIGHT - y2, 255);
	}
}

//
// Advance the trails in fixed steps. Lines are drawn into a framebuffer that
// is never cleared and blurred once per step, so how long they linger follows
// the step rate.
//
void DEMO_FixedUpdate(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * LINE_SPEED, LINE_PERIOD);

	// Calculate movement
	double aa = phase / 1.37;
	double rx = fabs(sin(sin(phase / 4.1) * M_PI) * 90) + 9;
	double ry = fabs(cos(cos(phase / 1.3) * M_PI) * 90) + 9;
	double xx = cos(cos(phase / 2.0) * M_PI) * rx;
	double yy = sin(cos(phase / 2.7) * M_PI) * ry;

	float x = RETRO_WIDTH / 2.0 + xx * cos(aa) + yy * sin(aa);
	float y = RETRO_HEIGHT / 2.0 - xx * sin(aa) + yy * cos(aa);
	float k = sin(phase / 15.0) + 0.75;

	// Draw lines
	DrawLines(x, y, k);

	RETRO_Blur(RETRO_BLUR_FIRE, sin(aa / 8.0) * 4 + 5);
}

void DEMO_Initialize(void)
{
	// Init palette. Blue rises evenly across the palette, while a narrow white
	// flare peaks halfway up and a glow washes in the last quarter
	RETRO_SetColor(0, RETRO_BLACK);
	for (int i = 1; i < RETRO_COLORS; i++) {
		float flare = pow(sin(M_PI * i / 511.0), 16) * 128;
		float glow = 32.0 * pow((float)i / 256.0, 3);
		unsigned char white = MIN(flare + glow, 63) * 4;
		RETRO_SetColor(i, white, white, i);
	}
}
