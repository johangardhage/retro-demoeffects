//
// Linedance 3
//
// A chain of POINTS attractors. Point 0 is a wandering walker; each later
// point is
//
//   p_i' = (p_i + p_{i-1}) / (2 + k / POINTS)
//
// k = 0 is the midpoint. k/POINTS is a small shrink (k > 0) or swell
// (k < 0) toward the origin. k = 0.75 + sin(phase / 15) ∈ [−0.25, 1.75].
// Segments are drawn in all four mirror quadrants. The framebuffer is
// never cleared; a fire blur after each step is what fades the trails.
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
#include "lib/retrocolor.h"

#define POINTS 170
#define LINE_SPEED 2.5
#define LINE_PERIOD (2 * M_PI * 19715670.0) // 2π · 2 · 3³ · 5 · 13 · 41 · 137

Point2Df Points[POINTS];

void DrawLines(int x, int y, float k)
{
	Points[0].x = x;
	Points[0].y = y;

	for (int i = 1; i < POINTS; i++) {
		Points[i].x = (Points[i].x + Points[i - 1].x) / (2.0 + k / POINTS);
		Points[i].y = (Points[i].y + Points[i - 1].y) / (2.0 + k / POINTS);

		int x1 = CLAMPWIDTH(Points[i].x);
		int x2 = CLAMPWIDTH(Points[i - 1].x);
		int y1 = CLAMPHEIGHT(Points[i].y);
		int y2 = CLAMPHEIGHT(Points[i - 1].y);

		RETRO_DrawLine(x1, y1, x2, y2, 255);
		RETRO_DrawLine(x1, (RETRO_HEIGHT - 1) - y1, x2, (RETRO_HEIGHT - 1) - y2, 255);
		RETRO_DrawLine((RETRO_WIDTH - 1) - x1, y1, (RETRO_WIDTH - 1) - x2, y2, 255);
		RETRO_DrawLine((RETRO_WIDTH - 1) - x1, (RETRO_HEIGHT - 1) - y1, (RETRO_WIDTH - 1) - x2, (RETRO_HEIGHT - 1) - y2, 255);
	}
}

//
// Advance the trails in fixed steps. Lines are drawn into a framebuffer that is never
// cleared and blurred once per step, so how long they linger follows the step rate.
//
void DEMO_Update(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * LINE_SPEED, LINE_PERIOD);

	// Calculate movement
	double aa = phase / 1.37;
	double rx = fabs(sin(sin(phase / 4.1) * M_PI) * 90) + 9;
	double ry = fabs(cos(cos(phase / 1.3) * M_PI) * 90) + 9;
	double xx = cos(cos(phase / 2.0) * M_PI) * rx;
	double yy = sin(cos(phase / 2.7) * M_PI) * ry;

	int x = RETRO_WIDTH / 2 + xx * cos(aa) + yy * sin(aa);
	int y = RETRO_HEIGHT / 2 - xx * sin(aa) + yy * cos(aa);
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
