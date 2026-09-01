//
// Six balls
//
// Six flat, differently colored balls sit at the corners of a regular hexagon.
// The hexagon tumbles as one rigid plane while its centre moves back and forth
// through the camera. Perspective turns the ring into a line, a flower, six
// huge overlapping color fields, and a small distant wheel -- the sequence
// used in Sanity's Interference.
//
// The balls are billboards: their centres are projected as 3-D points, but the
// discs always face the screen. Their radii use the same perspective divide as
// their centres. Sorting the six centres from back to front makes overlaps read
// consistently without needing a depth buffer.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrogfx.h"

#define BALLS 6
#define RING_RADIUS 47.0f
#define BALL_RADIUS 20.0f
#define CYCLE 10.0f // seconds for one flight past the camera and back
#define ROTATEX 0.63f // radians a second
#define ROTATEY 0.91f
#define ROTATEZ 0.37f
#define NEAR_Z -215.0f
#define FAR_Z 285.0f
#define DRIFT_X 22.0f
#define DRIFT_Y 13.0f

Vertex Balls[BALLS];

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation and phase. The flight wraps on the cycle and the tumble
	// on a turn, each taken from the clock directly because the two have nothing
	// to do with each other: driving the angles from the cycle's own wrapped
	// phase would snap the hexagon back to where it started every time the
	// flight came round, and only a rate that completed a whole number of turns
	// in CYCLE seconds would hide it.
	float ax = fmod(time * ROTATEX, 2 * M_PI);
	float ay = fmod(time * ROTATEY, 2 * M_PI);
	float az = fmod(time * ROTATEZ, 2 * M_PI);
	float phase = fmod(time, CYCLE) * 2.0f * M_PI / CYCLE;

	// Ease at both ends of the trip. The small sideways loop prevents the six
	// projected centres from expanding forever around one perfectly fixed point.
	float travel = 0.5f - 0.5f * cos(phase);
	float zoffset = FAR_Z + (NEAR_Z - FAR_Z) * travel;
	float cx = RETRO_WIDTH / 2.0f + DRIFT_X * sin(phase);
	float cy = RETRO_HEIGHT / 2.0f + DRIFT_Y * sin(phase * 2.0f + 0.7f);

	for (int i = 0; i < BALLS; i++) {
		float a = i * 2.0f * M_PI / BALLS;
		Balls[i].x = RING_RADIUS * cos(a);
		Balls[i].y = RING_RADIUS * sin(a);
		Balls[i].z = 0;
		RETRO_RotateVertex(&Balls[i], ax, ay, az);
		Balls[i].rz += zoffset;
		RETRO_ProjectVertex(&Balls[i], 1.0f, cx, cy);
	}

	// Painter's order: larger rz is farther from the eye.
	int order[BALLS] = { 0, 1, 2, 3, 4, 5 };
	for (int i = 1; i < BALLS; i++) {
		int ball = order[i];
		int j = i;
		while (j > 0 && Balls[order[j - 1]].rz < Balls[ball].rz) {
			order[j] = order[j - 1];
			j--;
		}
		order[j] = ball;
	}

	for (int i = 0; i < BALLS; i++) {
		Vertex *ball = &Balls[order[i]];
		if (ball->q == 0.0f) {
			continue;
		}
		float radius = BALL_RADIUS * RETRO_PROJECTION_EYEDISTANCE * ball->q;
		RETRO_DrawEllipse(ball->sx, ball->sy, radius, radius, order[i] + 1);
	}
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, 45, 47, 78);   // blue-black backdrop
	RETRO_SetColor(1, 184, 124, 113); // dusty coral
	RETRO_SetColor(2, 108, 174, 177); // pale cyan
	RETRO_SetColor(3, 111, 169, 110); // green
	RETRO_SetColor(4, 174, 111, 174); // mauve
	RETRO_SetColor(5, 178, 173, 105); // sand
	RETRO_SetColor(6, 180, 61, 108);  // magenta
}
