//
// Dot torus
//
// A torus of points, rotated and projected, each shaded by depth. The far
// side is drawn too, and only its shade sets it apart. That shade is
// monotone in depth, so keeping the brighter of two dots on one pixel is an
// exact depth test.
//
// Points sit on a grid of the two angles. DOT_SPACING does not divide
// either circumference, so the steps are 2π/n_α and 2π/n_β: even arc on
// each generating circle, both close, no leftover gap.
//
//   (x, y, z) = ((R + r cos β) cos α,  r sin β,  (R + r cos β) sin α)
//
// That is even in arc length on those circles, not in area. The area
// element carries (R + r cos β), so the rings crowd toward the hole - three
// times as densely at the inner rim, since R = 2r here. That crowding is the
// intended shape, not an error.
//
// Depth toward the viewer is z = -rz. No point is further from the centre
// than R+r, so the ramp covers [-R-r, R+r]:
//
//   color = (z + R+r) * (SHADES - 1) / (2(R+r))
//
// Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropalette.h"

#define RING_RADIUS 50 // from the axis out to the middle of the tube
#define TUBE_RADIUS 25 // and the radius of the tube itself
#define DOT_SPACING 5 // pixels between neighbouring dots, the same both ways around
#define SHADES 64 // palette entries the depth shading ramps over
#define ROTATION_SPEED 1 // radians per second, so about a turn every six seconds
#define PROJECTION_SCALE 1.0 // the torus is built in pixels, so the projection adds no scale

#define RING_STEPS ((int)(2 * M_PI * RING_RADIUS / DOT_SPACING + 0.5))
#define TUBE_STEPS ((int)(2 * M_PI * TUBE_RADIUS / DOT_SPACING + 0.5))

int NumPoints = 0;
Vertex Torus[RING_STEPS * TUBE_STEPS];

void DEMO_Render(double deltatime)
{
	// Calculate rotation
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	int furthest = RING_RADIUS + TUBE_RADIUS;

	// Draw points
	for (int i = 0; i < NumPoints; i++) {
		RETRO_RotateVertex(&Torus[i], ax, ay, az);
		RETRO_ProjectVertex(&Torus[i], PROJECTION_SCALE);

		int x = Torus[i].sx;
		int y = Torus[i].sy;
		int z = -round(Torus[i].rz);

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			int color = (z + furthest) * (SHADES - 1) / (2 * furthest);

			if (color > RETRO_GetPixel(x, y)) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, SHADES, RETRO_BLACK, RETRO_WHITE);

	// Init points. α around the ring, β around the tube.
	float alphastep = 2 * M_PI / RING_STEPS;
	float betastep = 2 * M_PI / TUBE_STEPS;

	for (int a = 0; a < RING_STEPS; a++) {
		for (int b = 0; b < TUBE_STEPS; b++) {
			float alpha = a * alphastep;
			float beta = b * betastep;
			float ring = RING_RADIUS + TUBE_RADIUS * cos(beta);

			Torus[NumPoints].x = ring * cos(alpha);
			Torus[NumPoints].y = TUBE_RADIUS * sin(beta);
			Torus[NumPoints].z = ring * sin(alpha);
			NumPoints++;
		}
	}
}
