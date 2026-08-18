//
// Dot ball
//
// A sphere of points, rotated and projected. Only the near cap is drawn, shaded by
// depth, so the ball reads as solid without a surface.
//
// Points sit on a latitude/longitude grid, one step of POINTSTEP radians in each
// angle:
//
//   (x, y, z) = R (cos α sin β,  cos β,  sin α sin β)
//
// Steps are 2π/n_α and π/n_β so the grid closes (POINTSTEP itself does not
// divide the circle). That is even in angle, not over the surface: the area
// of a band carries a sin(β) that stepping β evenly ignores, so the rings
// tighten at the poles. That crowding is the intended shape, not an error.
//
// Depth toward the viewer is z = -rz. The drawn cap is z > ZMIN, shaded
//
//   color = (z - ZMIN) * (SHADES - 1) / (R - ZMIN)
//
// so the rim of the cap is dark and the point facing the viewer is white.
// That shade is strictly increasing in depth, so the framebuffer doubles as
// a depth buffer: where two dots land on one pixel, keeping the brighter is
// an exact depth test.
//
// Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrocolor.h"

#define RADIUS 75
#define POINTSTEP 0.1 // radians between points, in both angles
#define SHADES 64 // palette entries the depth shading ramps over
#define ZMIN 20 // points nearer the viewer than this are drawn, which hides the far side
#define ROTATION_SPEED 1 // radians per second, so about a turn every six seconds
#define PROJECTION_SCALE 1.0 // the ball is built in pixels, so the projection adds no scale

#define ALPHA_STEPS ((int)(2 * M_PI / POINTSTEP + 0.5))
#define BETA_STEPS ((int)(M_PI / POINTSTEP + 0.5))

int NumPoints = 0;
Vertex Ball[ALPHA_STEPS * (BETA_STEPS + 1)];

void DEMO_Render(double deltatime)
{
	// Calculate rotation
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	// Draw points
	for (int i = 0; i < NumPoints; i++) {
		RETRO_RotateVertex(&Ball[i], ax, ay, az);
		RETRO_ProjectVertex(&Ball[i], PROJECTION_SCALE);

		int x = Ball[i].sx;
		int y = Ball[i].sy;
		int z = -round(Ball[i].rz);

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT && z > ZMIN) {
			int color = (z - ZMIN) * (SHADES - 1) / (RADIUS - ZMIN);

			// Grid order is fixed at startup in model space and depth is the
			// rotated z, so no draw order can be back to front
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

	// Init points. POINTSTEP does not divide 2π or π, so the step is
	// 2π/n_α and π/n_β instead: even in angle, both poles included,
	// no leftover gap and no doubled meridian.
	float alphastep = 2 * M_PI / ALPHA_STEPS;
	float betastep = M_PI / BETA_STEPS;

	for (int a = 0; a < ALPHA_STEPS; a++) {
		for (int b = 0; b <= BETA_STEPS; b++) {
			float alpha = a * alphastep;
			float beta = b * betastep;

			Ball[NumPoints].x = RADIUS * cos(alpha) * sin(beta);
			Ball[NumPoints].y = RADIUS * cos(beta);
			Ball[NumPoints].z = RADIUS * sin(alpha) * sin(beta);
			NumPoints++;
		}
	}
}
