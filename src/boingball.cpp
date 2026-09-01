//
// Boing ball
//
// The Amiga's own demo, from the 1984 CES: a checkered ball bouncing in a
// gridded room. The checker is not a texture but the face list of
// assets/spherequads.obj, where face f is row f / MERIDIANS and column
// f % MERIDIANS and the parity of their sum picks one of two colors. It lines
// up around the seam because the meridians are even. Nothing is lit, so the
// ball reads as round only from the way the checks foreshorten towards the
// silhouette, as in the original.
//
// RETRO_RotateModel builds R = Rz Ry Rx, so with ax = 0 the spin about y
// happens before the tilt about z: a ball turning about a leaning axis, not a
// leaning ball turning upright. It is not rolling, and never was.
//
// One phase over [0, 1) drives both motions, SWEEPS crossings of the room and
// BOUNCES bounces,
//
//   x = CX + AMP (2 |2 s - 1| - 1),  s = frac(phase SWEEPS)
//   y = FLOOR - HEIGHT (1 - w²),     w = 2 frac(phase BOUNCES) - 1
//
// a triangle at constant speed, reversing at the walls along with the spin,
// and the parabola of a free fall the floor returns at the speed it arrived
// at. Whole counts wrap the phase without a seam, and coprime ones put the
// k-th landing at frac(k SWEEPS / BOUNCES) of a crossing, so the ball works
// through several of them before it repeats.
//
// The shadow is the ball projected a second time. RETRO_ProjectModel's screen
// centre is an offset in pixels applied after the divide, so it moves the ball
// on the wall without moving it in z, keeping its size and outline.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"
#include "lib/retrogfx.h"

#define BOING_MERIDIANS 16 // faces around the ball, and columns of checks
#define BOING_SCALE 45 // a unit sphere comes out this many pixels in radius
#define BOING_TILT 0.30 // radians the spin axis leans, about z
#define BOING_SPIN 2.4 // radians a second about that axis
#define BOING_PERIOD 9.6 // seconds of motion before the phase repeats
#define BOING_SWEEPS 2 // crossings of the room and back in that time
#define BOING_BOUNCES 7 // floor bounces in it, coprime with SWEEPS so the landings differ
#define BOING_CX (RETRO_WIDTH / 2)
#define BOING_AMP 95 // pixels either side of centre the ball travels
#define BOING_FLOOR 190 // screen y of the ball's centre when it is on the floor
#define BOING_HEIGHT 70 // pixels the ball rises at the top of a bounce
#define BOING_SHADOWX 26 // pixels the shadow sits to the right of the ball
#define BOING_SHADOWY 14 // and below it
#define BOING_GRIDX 20 // pixels between the grid's vertical lines
#define BOING_GRIDY 20 // and its horizontal ones

// Palette. The ball and the shadow are both drawn with face colors 0 and 1,
// and the model's base color picks which pair those land in, so the shadow is
// the same two entries set to the same gray
#define BOING_BACKGROUND 0
#define BOING_GRID 1
#define BOING_SHADOW 2 // and 3, the two checks of the shadow, both one color
#define BOING_BALL 4 // and 5, the white check and the red one

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime / BOING_PERIOD, 1.0);

	// Cross the room and come back, bouncing on the way
	double sweep = fmod(phase * BOING_SWEEPS, 1.0);
	float w = 2 * fmod(phase * BOING_BOUNCES, 1.0) - 1;
	int x = lround(BOING_CX + BOING_AMP * (2 * fabs(2 * sweep - 1) - 1));
	int y = lround(BOING_FLOOR - BOING_HEIGHT * (1 - w * w));

	// Spin, reversing with the direction of travel so the ball leaves each wall
	// turning the other way
	static float ay;
	float direction = sweep < 0.5 ? -1.0f : 1.0f;
	ay = fmod(ay + direction * deltatime * BOING_SPIN, 2 * M_PI);

	// The room: a flat grid on the back wall, redrawn every frame under the ball
	for (int gx = 0; gx < RETRO_WIDTH; gx += BOING_GRIDX) {
		RETRO_DrawVline(gx, 0, RETRO_HEIGHT - 1, BOING_GRID);
	}
	for (int gy = 0; gy < RETRO_HEIGHT; gy += BOING_GRIDY) {
		RETRO_DrawLine(0, gy, RETRO_WIDTH - 1, gy, BOING_GRID);
	}

	// Draw the ball twice from one rotation: the shadow on the wall behind, then
	// the ball over it
	Model3D *model = RETRO_Get3DModel();
	RETRO_RotateModel(0, ay, BOING_TILT);
	model->c = BOING_SHADOW;
	RETRO_ProjectModel(BOING_SCALE, x + BOING_SHADOWX, y + BOING_SHADOWY);
	RETRO_RenderModel(RETRO_POLY_FLAT);
	model->c = BOING_BALL;
	RETRO_ProjectModel(BOING_SCALE, x, y);
	RETRO_RenderModel(RETRO_POLY_FLAT);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(BOING_BACKGROUND, RETRO_LIGHTGRAY);
	RETRO_SetColor(BOING_GRID, RETRO_DEEPPINK);
	RETRO_SetColor(BOING_SHADOW, RETRO_GRAY);
	RETRO_SetColor(BOING_SHADOW + 1, RETRO_GRAY);
	RETRO_SetColor(BOING_BALL, RETRO_WHITE);
	RETRO_SetColor(BOING_BALL + 1, RETRO_RED);

	// The checker is the face list: row + column, odd or even
	Model3D *model = RETRO_Load3DModel("assets/spherequads.obj");
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = (i / BOING_MERIDIANS + i % BOING_MERIDIANS) & 1;
	}
}
