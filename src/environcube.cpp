//
// Environment mapped cube
//
// The same room as environsphere.cpp, reflected in a cube. cube.obj carries
// only its eight corner normals (the diagonals, 70° apart), so each flat face
// interpolates between three or four of them and the checker and sky warp
// within a face, where the sphere's hundred-odd fold them smoothly. A face
// is a piece of a chrome ball rather than a plane mirror: flat mirrors, with
// one normal a face, would show a slice of the room a few tens of degrees
// wide, and in this room that is mostly one sky shade. A true reflection,
// not a lighting map (RETRO_POLY_MATCAP, see matcapcube.cpp):
// RETRO_POLY_ENVIRONMENT indexes ChromeMap by the pixel's own interpolated
// normal and writes what it finds straight to the framebuffer, no palette
// or Lambert term involved.
//
// The reflection ray is derived in environsphere.cpp and the room is the
// same floor and sky, but the cube cannot use the ball's bake of it. The
// ball shows each ring of the map at its own radius; a cube face stretches
// a few rings across its whole width, and magnifies each texel into several
// pixels. Whatever the ball keeps thin becomes a band here:
//
// - The dark square is lifted off the black background, since the lower
//   faces are mostly floor and a black square cut into the cube's outline.
// - The outer ring of the map is squeezed inwards, see ChromeEase, since an
//   edge-on face reads it across its whole width.
// - The checker is point-sampled, not box-filtered: a filtered edge is a
//   gray texel, magnified into a gray line. Where squares get too small to
//   point-sample, near the horizon, the floor fades into the horizon colour
//   instead of averaging into the ball's gray band.
// - The checker sits half a square off the origin, so the ray straight down,
//   which a face turned to the floor shows across much of its width, hits
//   the middle of a square, not the corner of four.
//
// Unlike the ball, the cube is not its own rotation: turning it changes
// which faces and which corners of them face the camera, so it turns like
// any other cube demo instead of holding still and orbiting.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define CHROME_SIZE 512 // side of the square reflection map, in texels; a face magnifies it, and 256 steps the square edges
#define CHROME_EYE_HEIGHT 1.5f // room units the cube's centre sits above the floor
#define CHROME_FLOOR_TILE 1.5f // room units a checker square spans
#define CHROME_FLOOR_SHADES 2 // CHROME_FLOOR_DARK and white; the checker is point-sampled, so nothing in between
#define CHROME_SKY_SHADES 48
#define CHROME_HAZE_LEVELS 64 // steps from the clear checker to the horizon colour
#define CHROME_HAZE_FOOTPRINT 0.3f // squares a pixel covers before the checker starts to shimmer and is hazed over; fully at twice this
#define CHROME_HAZE_DISTANCE 12.0f // room units at which the floor is 1 - 1/e of the way into the horizon; squared, so the near floor stays clear

#define CHROME_FLOOR_DARK RETRO_Palette{ 40, 40, 48 } // the dark square, lifted off CHROME_BG so the cube's outline survives it
#define CHROME_HORIZON RETRO_Palette{ 168, 198, 224 } // pale blue at Ry = 0, where the hazed floor meets the sky

#define CHROME_BG 0
#define CHROME_FLOOR_START 1
#define CHROME_SKY_START (CHROME_FLOOR_START + CHROME_FLOOR_SHADES * CHROME_HAZE_LEVELS)

#define ROTATION_SPEED 2 // radians a second, about each axis

static unsigned char ChromeMap[CHROME_SIZE * CHROME_SIZE];

// Floor-plane tile coordinates of the reflection of N = (px, py, -sqrt), or
// false when the texel is off the disk or the ray does not look down
static bool ChromeFloorTile(float px, float py, float &x, float &z)
{
	float r2 = px * px + py * py;
	if (r2 > 1.0f) return false;
	float nz = sqrt(1.0f - r2);
	float ry = 2.0f * nz * py;
	if (ry >= -1.0e-4f) return false;
	float t = CHROME_EYE_HEIGHT / -ry;
	x = t * (2.0f * nz * px) / CHROME_FLOOR_TILE;
	z = t * (2.0f * r2 - 1.0f) / CHROME_FLOOR_TILE;
	return true;
}

// The rim is a horizontal ray. The ball only shows it at its outline, but a
// cube face turned edge-on interpolates its normals through it and past it,
// where the lookup turns back into the disk, and each texel there jumps
// between horizon, floor and zenith: a torn band along the far edges. Squeeze
// |Nxy| from 0.8 out into 0.9 instead, meeting the unsqueezed map with the
// same slope at 0.8 and flattening out at the rim. Never turning back matters:
// a ring folded back on itself shows each square twice, mirrored into a
// rounded tile.
static void ChromeEase(float px, float py, float &qx, float &qy)
{
	qx = px;
	qy = py;
	float r2 = px * px + py * py;
	if (r2 > 0.64f) {
		float radius = sqrt(r2);
		float t = CLAMP01((radius - 0.8f) / 0.2f);
		float eased = 0.8f + 0.1f * t * (2.0f - t);
		float s = eased / radius;
		qx = px * s;
		qy = py * s;
	}
}

static void BuildChromeMap(void)
{
	float c = (CHROME_SIZE - 1) * 0.5f;
	// One screen pixel of a unit ball drawn at this many pixels of radius, in
	// normal-xy: the ball's footprint, and a generous one for a cube face
	float step = 1.0f / RETRO_PROJECTION_SCALE;

	for (int y = 0; y < CHROME_SIZE; y++) {
		// -Ny for the row. A front face reads the lower rows at Ny > 0 (down);
		// ry < 0 then paints those rows as the floor
		float py = -(y - c) / c;
		for (int x = 0; x < CHROME_SIZE; x++) {
			float px = (x - c) / c;
			float qx, qy;
			ChromeEase(px, py, qx, qy);
			float r2 = qx * qx + qy * qy;

			unsigned char index = 0;
			if (r2 <= 1.0f) {
				float nz = sqrt(1.0f - r2);
				float ry = 2.0f * nz * qy;

				if (ry >= 0.0f) {
					// sqrt: a linear ramp of Ry is horizon-white over most of the cap
					int shade = (int)(sqrt(ry) * CHROME_SKY_SHADES + 0.5f);
					index = CHROME_SKY_START + CLAMP(shade, 0, CHROME_SKY_SHADES);
				} else {
					// The footprint is one screen pixel's step taken before the
					// easing, where the renderer looks up, so the squeezed rim
					// reads as the few squares it really covers. A ray whose
					// neighbour falls off the floor has no footprint and is
					// all horizon.
					float fx, fz, fx1, fz1, fx2, fz2;
					float qx1, qy1, qx2, qy2;
					ChromeEase(px + step, py, qx1, qy1);
					ChromeEase(px, py + step, qx2, qy2);
					float shown = 0.5f;
					float footprint = 1.0e9f;
					if (ChromeFloorTile(qx, qy, fx, fz) && ChromeFloorTile(qx1, qy1, fx1, fz1) && ChromeFloorTile(qx2, qy2, fx2, fz2)) {
						float wx = MAX(fabs(fx1 - fx), fabs(fx2 - fx));
						float wz = MAX(fabs(fz1 - fz), fabs(fz2 - fz));
						footprint = MAX(wx, wz);
						// Point-sampled, so the map only ever holds the two
						// squares: a baked in-between value is one texel, and a
						// face magnifies a texel into a line of gray. Half a
						// square off the origin, because the ray straight down
						// would otherwise land where four squares meet.
						int parity = (int)floor(fx + 0.5f) + (int)floor(fz + 0.5f);
						shown = (parity & 1) ? 0.0f : 1.0f;
					}
					// Haze: the floor fades into the horizon colour with the
					// distance along the ray, and wherever a pixel covers so
					// much of a square that point-sampling would shimmer. The
					// far squares end in the sky's first entry, where a
					// filtered checker would average them into a gray band.
					float distance = CHROME_EYE_HEIGHT / -ry;
					float fog = distance / CHROME_HAZE_DISTANCE;
					float haze = MAX(1.0f - exp(-fog * fog), CLAMP01((footprint - CHROME_HAZE_FOOTPRINT) / CHROME_HAZE_FOOTPRINT));
					int level = (int)(haze * (CHROME_HAZE_LEVELS - 1) + 0.5f);
					int shade = (int)(shown * (CHROME_FLOOR_SHADES - 1) + 0.5f);
					index = CHROME_FLOOR_START + CLAMP(level, 0, CHROME_HAZE_LEVELS) * CHROME_FLOOR_SHADES + CLAMP(shade, 0, CHROME_FLOOR_SHADES);
				}
			}
			ChromeMap[y * CHROME_SIZE + x] = index;
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_ENVIRONMENT);
}

void DEMO_Initialize(void)
{
	// Init palette: a stark checker so the cube reads as a mirror rather than
	// a lit surface, and a sky ramp from a pale horizon to a deep zenith
	RETRO_Palette palette[RETRO_COLORS];
	memset(palette, 0, sizeof(palette));
	RETRO_SetColor(CHROME_BG, RETRO_BLACK, palette);
	// The two squares, each mixed further into the horizon a haze level at a
	// time, the last one entirely. The sky ramp starts at the next index with
	// the same colour, so the floor meets the sky without a seam.
	for (int j = 0; j < CHROME_HAZE_LEVELS; j++) {
		float h = (float)j / (CHROME_HAZE_LEVELS - 1);
		for (int i = 0; i < CHROME_FLOOR_SHADES; i++) {
			float k = (float)i / (CHROME_FLOOR_SHADES - 1);
			RETRO_Palette color;
			color.r = CHROME_FLOOR_DARK.r + (RETRO_WHITE.r - CHROME_FLOOR_DARK.r) * k;
			color.g = CHROME_FLOOR_DARK.g + (RETRO_WHITE.g - CHROME_FLOOR_DARK.g) * k;
			color.b = CHROME_FLOOR_DARK.b + (RETRO_WHITE.b - CHROME_FLOOR_DARK.b) * k;
			color.r = color.r + (CHROME_HORIZON.r - color.r) * h;
			color.g = color.g + (CHROME_HORIZON.g - color.g) * h;
			color.b = color.b + (CHROME_HORIZON.b - color.b) * h;
			RETRO_SetColor(CHROME_FLOOR_START + j * CHROME_FLOOR_SHADES + i, color, palette);
		}
	}
	RETRO_CreateGradientPalette(CHROME_SKY_START, CHROME_SKY_START + CHROME_SKY_SHADES, CHROME_HORIZON, RETRO_MIDNIGHTBLUE, palette);
	RETRO_SetPalette(palette);

	// Build the reflection map once; the room is environsphere.cpp's
	BuildChromeMap();

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->envmap = ChromeMap;
	model->envmapwidth = CHROME_SIZE;
	model->envmapheight = CHROME_SIZE;
}
