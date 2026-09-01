//
// Dot sphere
//
// A random cloud on the unit sphere, rotated and projected. Each point stamps
// a 5×5 additive splat; a 4-neighbour diffuse blur (no self) then smears the
// frame. DEMO_Render clears first, so that smear is spatial, not a trail.
//
// Points are uniform on the sphere, not uniform in angle. A zone of height
// dz has area 2π dz, independent of latitude, so z uniform on [−1, 1] and
// φ uniform on [0, 2π) is the uniform measure:
//
//   z = 2u − 1,   φ = 2π v,   r = √(1 − z²)
//   (x, y, z) = (r cos φ, r sin φ, z)
//
// r is the parallel at height z. Uniform polar angle ρ would clump at the
// poles: the area element is sin ρ dρ dφ, so the density is 1/sin ρ, and
// ρ over [0, 2π] would cover the sphere twice. The sample is random, so it
// clumps locally; that cloud is the intended shape, not an error.
//
// The turn is sequential Rx, Ry, Rz. Equal rates on all three axes still
// tumble; a single-axis spin would leave two angles at zero. Euler angles
// live on 2π.
//
// Projection is the pinhole, s = 75, eye = 250:
//
//   depth = s rz + eye
//   sx    = cx + s rx eye / depth
//   sy    = cy + s ry eye / depth
//
// At the equator (rz = 0) a model unit is s pixels. Depth toward the viewer
// is −rz, so the near pole sits at depth 175 and the far pole at 325. The
// splat is always 5×5 pixels, so only the centres have perspective.
//
// Both hemispheres are drawn, with no depth test. The surface is edge-on at
// the silhouette, so projected density rises toward the limb
// (orthographically 2 / |z| = 2 / √(1 − x² − y²)) and stamps add there.
// The kernel is a 5×5 diamond, 31 at the centre down to 17, corners empty.
// The blur is one in-place pass
//
//   T' = mean(N, W, E, S) − 3
//
// with no self term, so a splat becomes a halo rather than a softer dot.
// The palette is two ramps, black–sage–black then black–rose: a splat
// (17–31) sits on the sage shoulder, overlaps fall through black into rose.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define NUM_POINTS 2048
#define SPHERE_SCALE 75 // the sphere is built at unit size, so the projection scales it to pixels
#define ROTATION_SPEED 0.6 // radians per second
#define BLOB_SIZE 5 // the splat stamped at each point, in pixels
#define BLUR_DECAY 3 // brightness the blur takes off, so the smear stays a halo

unsigned char Blob[BLOB_SIZE][BLOB_SIZE] = {
	{ 0, 17, 22, 17,  0},
	{17, 26, 29, 26, 17},
	{22, 29, 31, 29, 22},
	{17, 26, 29, 26, 17},
	{ 0, 17, 22, 17,  0}
};

Vertex Shape[NUM_POINTS];

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	for (int i = 0; i < NUM_POINTS; i++) {
		RETRO_RotateVertex(&Shape[i], ax, ay, az);
		RETRO_ProjectVertex(&Shape[i], SPHERE_SCALE);

		int x = Shape[i].sx;
		int y = Shape[i].sy;

		for (int by = 0; by < BLOB_SIZE; by++) {
			for (int bx = 0; bx < BLOB_SIZE; bx++) {
				if (Blob[by][bx] == 0) {
					continue;
				}

				int px = x + bx - BLOB_SIZE / 2;
				int py = y + by - BLOB_SIZE / 2;
				if (px < 0 || px >= RETRO_WIDTH || py < 0 || py >= RETRO_HEIGHT) {
					continue;
				}

				int color = RETRO_GetPixel(px, py) + Blob[by][bx];
				RETRO_PutPixel(px, py, CLAMP(color, 0, RETRO_COLORS));
			}
		}
	}

	RETRO_Blur(RETRO_BLUR_DIFFUSE, BLUR_DECAY);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 16, RETRO_BLACK, RETRO_SAGE);
	RETRO_CreateGradientPalette(16, 53, RETRO_SAGE, RETRO_BLACK);
	RETRO_CreateGradientPalette(53, RETRO_COLORS, RETRO_BLACK, RETRO_ROSE);

	// Init sphere
	for (int i = 0; i < NUM_POINTS; i++) {
		float z = 2.0f * RANDOMF(1) - 1.0f;
		float phi = RANDOMF(2 * M_PI);
		float r = sqrtf(1.0f - z * z);
		Shape[i].x = r * cosf(phi);
		Shape[i].y = r * sinf(phi);
		Shape[i].z = z;
	}
}
