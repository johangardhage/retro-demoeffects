//
// Star flight: cruise, turn onto a new heading, then roll through the stars.
// Stars wrap in a world-aligned box around the moving eye. A spherical fade
// hides the wrapping, even when looking diagonally through the box.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocamera.h"
#include "lib/retropalette.h"

#define NUM_STARS 4000
#define SPEED 600 // world units per second
#define STAR_FAR 600
#define STAR_NEAR 8
#define EYE 250
#define SHADES 64
#define FLIGHT_PERIOD 10.0

Vertex Stars[NUM_STARS]; // World position in x, y, z; view and screen space filled per frame

static RETRO_Camera CameraStart = { {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
static long long FlightCycle = -1;
static int TurnDirection = 0; // right, left, up, down

// Turned from CameraStart by angle, about the axis TurnDirection names -
// down for a left/right turn, right for an up/down one - so up/down stays
// correct after any sequence of turns.
static RETRO_Camera Turn(double angle)
{
	RETRO_Camera camera = CameraStart;
	if (TurnDirection == 0) {
		RETRO_YawCamera(&camera, angle);
	} else if (TurnDirection == 1) {
		RETRO_YawCamera(&camera, -angle);
	} else if (TurnDirection == 2) {
		RETRO_PitchCamera(&camera, -angle);
	} else {
		RETRO_PitchCamera(&camera, angle);
	}
	return camera;
}

// Smooth starts and stops, including acceleration at phase boundaries.
static double Ease(double t)
{
	t = CLAMP01(t);
	return t * t * t * (t * (t * 6 - 15) + 10);
}

static double Wrap(double value)
{
	return value - 2 * STAR_FAR * floor((value + STAR_FAR) / (2 * STAR_FAR));
}

void DEMO_Render(double time, double deltatime)
{
	long long cycle = (long long)floor(time / FLIGHT_PERIOD);
	double phase = time - cycle * FLIGHT_PERIOD;
	while (FlightCycle < cycle) {
		if (FlightCycle >= 0) {
			CameraStart = Turn(1.15);
		}
		TurnDirection = RANDOM(4);
		++FlightCycle;
	}
	// Turn from seconds two to seven; overlap with a faster roll from five to ten.
	// Choose turns from CameraStart so up/down remain correct after any turn.
	RETRO_Camera camera = Turn(1.15 * Ease((phase - 2) / 5));
	RETRO_RollCamera(&camera, 2 * M_PI * Ease((phase - 5) / 5));
	double travel = SPEED * deltatime;

	for (int i = 0; i < NUM_STARS; i++) {
		Vertex &star = Stars[i];
		// Move the eye along its forward vector; keep positions eye-relative.
		vec3 moved = star.pos - camera.forward * travel;
		star.pos = { (float)Wrap(moved.x), (float)Wrap(moved.y), (float)Wrap(moved.z) };

		RETRO_ViewVertex(&star, &camera);
		if (star.rpos.z <= STAR_NEAR) {
			continue;
		}
		double distance = length(star.pos);
		if (distance >= STAR_FAR) {
			continue;
		}

		RETRO_ProjectViewVertex(&star, EYE);
		int x = star.spos.x;
		int y = star.spos.y;
		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			int color = (SHADES - 1) * (1 - distance / STAR_FAR);
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_InitializeCamera(&CameraStart);
	FlightCycle = -1;
	RETRO_CreateGradientPalette(0, SHADES, RETRO_BLACK, RETRO_WHITE);
	for (int i = 0; i < NUM_STARS; i++) {
		Stars[i].pos = {
			(float)(RANDOM(2 * STAR_FAR) - STAR_FAR),
			(float)(RANDOM(2 * STAR_FAR) - STAR_FAR),
			(float)(RANDOM(2 * STAR_FAR) - STAR_FAR)
		};
	}
}
