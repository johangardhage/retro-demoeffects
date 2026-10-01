//
// Fractal tree
//
// A binary tree that grows from the ground, sways in the wind and shrinks
// back. A branch at depth d ends in two children, turned SPREAD to either
// side of it and scaled by SHRINK:
//
//   angle_child = angle ± SPREAD (1 + j) + sway_d
//   length_child = SHRINK length (1 + j)
//
// j is a small jitter per branch from RETRO_Hash of its depth and its index
// within that depth (2i and 2i + 1 for the children of i), so the tree is
// irregular but the same every frame. The wind bends every branch by
//
//   sway_d = SWAY (d + 1) / DEPTH sin(2π (WIND_SPEED t) − WIND_LAG d)
//
// so the twigs swing further than the trunk and the gust travels up the
// tree. Growth g runs from 0 to DEPTH and back: a branch at depth d is drawn
// at CLAMP01(g − d) of its length, so each level sprouts once the one below
// it is fully out. The trunk is BASE_WIDTH pixels thick and every level is
// a pixel thinner, down to one. The color steps from bark to leaf with depth.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"
#include "lib/retromath.h"

#define DEPTH 11 // levels of branches, the trunk included
#define TRUNK_LENGTH 50.0f // pixels
#define SHRINK 0.74f // length of a child relative to its parent
#define SPREAD radians(24) // turn of a child away from its parent
#define JITTER 0.25f // largest relative change the hash makes to a branch
#define SWAY radians(7) // the tips' bend at the peak of a gust
#define WIND_SPEED 0.35 // gusts a second
#define WIND_LAG 0.45 // radians of gust phase each level lags the one below
#define BASE_WIDTH 6 // pixels, the trunk's thickness
#define TIME_GROW 5.0 // seconds from a seed to the full tree
#define TIME_HOLD 6.0 // seconds the full tree stands
#define TIME_CYCLE (2 * TIME_GROW + TIME_HOLD)

static float Sway[DEPTH];
static float Growth;

//
// A line width pixels thick, stacked across whichever axis the line
// runs less along
//
static void DrawThickLine(int x1, int y1, int x2, int y2, int width, unsigned char color)
{
	bool steep = abs(y2 - y1) > abs(x2 - x1);

	for (int k = -(width - 1) / 2; k <= width / 2; k++) {
		if (steep) {
			RETRO_DrawLine(x1 + k, y1, x2 + k, y2, color);
		} else {
			RETRO_DrawLine(x1, y1 + k, x2, y2 + k, color);
		}
	}
}

//
// Draw the branch at depth, index within the depth, starting at (x, y) and
// heading along angle, then its children
//
static void DrawBranch(float x, float y, float angle, float length, int depth, int index)
{
	float grown = CLAMP01(Growth - depth);
	if (depth == DEPTH || grown == 0) {
		return;
	}

	float x2 = x + cos(angle) * length * grown;
	float y2 = y + sin(angle) * length * grown;
	int width = MAX(1, BASE_WIDTH - depth);
	unsigned char color = 1 + depth * (RETRO_COLORS - 2) / (DEPTH - 1);

	DrawThickLine(lround(x), lround(y), lround(x2), lround(y2), width, color);

	for (int side = 0; side < 2; side++) {
		int child = 2 * index + side;
		unsigned int hash = RETRO_Hash(depth + 1, child);
		float turn = JITTER * ((hash & 0xffff) / 32767.5f - 1);
		float scale = JITTER * ((hash >> 16) / 32767.5f - 1);
		float sign = side == 0 ? -1 : 1;

		float childangle = angle + sign * SPREAD * (1 + turn) + Sway[depth];
		DrawBranch(x2, y2, childangle, length * SHRINK * (1 + scale), depth + 1, child);
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase. The tree grows, stands, then shrinks back into the ground
	double phase = fmod(time, TIME_CYCLE);
	double grow = MIN(phase, TIME_CYCLE - phase) / TIME_GROW;
	Growth = MIN(grow, 1.0) * DEPTH;

	// A gust that starts at the trunk and travels up
	double wind = fract(time * WIND_SPEED) * 2 * M_PI;
	for (int depth = 0; depth < DEPTH; depth++) {
		Sway[depth] = SWAY * (depth + 1) / DEPTH * sin(wind - WIND_LAG * depth);
	}

	DrawBranch(RETRO_WIDTH / 2.0f, RETRO_HEIGHT - 1, -M_PI / 2, TRUNK_LENGTH, 0, 0);
}

void DEMO_Initialize(void)
{
	// Init palette. Bark at the trunk, through olive to fresh leaves at the tips
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(1, 90, RETRO_SADDLEBROWN, RETRO_SIENNA);
	RETRO_CreateGradientPalette(90, 170, RETRO_SIENNA, RETRO_OLIVEGRAY);
	RETRO_CreateGradientPalette(170, RETRO_COLORS, RETRO_FORESTGREEN, RETRO_SPRINGGREEN);
}
