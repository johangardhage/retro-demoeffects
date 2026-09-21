//
// Environment mapped sphere
//
// A true Blinn/Newell reflection map, not a lighting map: RETRO_POLY_ENVIRONMENT
// indexes ChromeMap by the pixel's own interpolated normal and writes what it
// finds straight to the framebuffer, no palette or Lambert term involved. See
// matcapcube.cpp's RETRO_POLY_MATCAP for the canned-highlight technique this
// is not, and environcube.cpp for flat mirrors, which reflect the true view
// ray and need a room with more in it than this floor and sky. Building the
// map, once, is the whole of this file.
//
// RETRO_GetEnvMapCoordinates folds a unit N to (u, v) by
//
//   u = W (1/2 + Nx / 2),  v = H (1/2 + Ny / 2)
//
// for a front face, Nz < 0. The library derives it from the sphere-mapped
// reflection R = 2(N·V)N - V, V = (0, 0, -1), which for a unit front face
// is that scale of Nxy. Nz > 0 is pushed out to the rim of the same disk,
// so a normal that crosses the silhouette does not jump to the far side.
// Baking the map runs that derivation the other way: texel (x, y) gives back
// the front-facing N it came from,
//
//   (px, py) = ((x - c)/c, -(y - c)/c),  c = (SIZE - 1)/2
//   N = (px, py, -sqrt(1 - px² - py²)),  px² + py² ≤ 1
//
// py negates the row's Ny. A front face has Nz < 0, so the v above is
// H (1/2 + Ny/2): the lower rows are the larger Ny, and +y is down, so a
// downward normal reads them. ry = 2 nz py and the floor test is ry < 0,
// so the negation paints those lower rows as downward rays and the floor
// sits under the ball. The bake fills only the front-facing N a visible
// surface reads. A normal past the silhouette samples the rim.
// R is then spelled out directly rather than folded, since here it is the
// answer, not an intermediate:
//
//   R = 2(N·V)N - V = (2 nz px,  2 nz py,  2(px² + py²) - 1),  nz = -N.z
//
// One ray per texel, cast from the ball's centre since the environment is far
// enough that the ball's own radius does not parallax it: Ry < 0 samples the
// checker floor CHROME_EYE_HEIGHT below, Ry ≥ 0 samples the sky ramp by how
// high the ray climbs. The sky runs from CHROME_HORIZON at the line to
// midnight blue at the zenith, on sqrt(Ry), so the pale blue stays a thin
// line rather than a white field. That line, and the rim where nz → 0, is a
// ray nearly parallel to the floor: a tile there is smaller than a screen
// pixel. The checker is box-filtered over one screen pixel of the ball (its
// radius is RETRO_PROJECTION_SCALE). Past that the sample is the average of
// the two squares, a mid-gray, which alone would leave a gray band under the
// line. The floor also fades into the horizon colour with the distance
// along the ray, squared so the near floor stays clear, and its last haze
// level is the sky's first entry, so the far floor meets the sky without a
// seam and the average is hazed over before it shows. A grazing normal is
// addressed with radius W/2, and the disk is baked at radius (W-1)/2, so
// that sample falls just outside. Pulling it back to |Nxy| = 1/sqrt(2) is
// the zenith above and the floor under the ball below. Left on the rim, the
// far top edge would be the pale horizon and the far bottom edge the haze.
//
// The ball itself never turns: a mirror sphere spinning about its own centre
// maps onto itself, so no rotation changes what any screen pixel reflects.
// It orbits across the floor instead, which does move the picture.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define CHROME_SIZE 256 // side of the square reflection map, in texels
#define CHROME_EYE_HEIGHT 1.5f // room units the ball's centre sits above the floor
#define CHROME_FLOOR_TILE 1.5f // room units a checker square spans
#define CHROME_FLOOR_SHADES 17 // black through white; the middle entry is the unresolved checker
#define CHROME_SKY_SHADES 48
#define CHROME_HAZE_LEVELS 12 // steps from the clear checker to the horizon colour; 17 x 12 floor entries fit the palette with the sky
#define CHROME_HAZE_DISTANCE 12.0f // room units at which the floor is 1 - 1/e of the way into the horizon; squared, so the near floor stays clear

#define ORBIT_SPEED 0.7f // radians a second
#define ORBIT_RADIUS_X 80 // pixels either side of centre
#define ORBIT_RADIUS_Y 34 // pixels above and below centre; smaller, an ellipse foreshortened onto the floor

#define CHROME_HORIZON RETRO_Palette{ 168, 198, 224 } // pale blue at Ry = 0, where the hazed floor meets the sky

#define CHROME_BG 0
#define CHROME_FLOOR_START 1
#define CHROME_SKY_START (CHROME_FLOOR_START + CHROME_FLOOR_SHADES * CHROME_HAZE_LEVELS)

static unsigned char ChromeMap[CHROME_SIZE * CHROME_SIZE];

// abs(fract(x) - 1/2), the distance from x to the centre of its unit cell, folded
static float ChromeFold(float x)
{
	float f = x - floor(x);
	return fabs(f - 0.5f);
}

// Integral of one checker axis across a box of width w, in tile units
static float ChromeBox(float p, float w)
{
	w = MAX(w, 1.0e-4f);
	return 2.0f * (ChromeFold((p - 0.5f * w) * 0.5f) - ChromeFold((p + 0.5f * w) * 0.5f)) / w;
}

// 0 is a black square, 1 a white one. w is the filter footprint in tile units,
// so a footprint wider than a square returns the gray in between.
static float ChromeChecker(float x, float z, float wx, float wz)
{
	return 0.5f - 0.5f * ChromeBox(x, wx) * ChromeBox(z, wz);
}

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

static void BuildChromeMap(void)
{
	float c = (CHROME_SIZE - 1) * 0.5f;
	// The unit ball is drawn at this many pixels of radius, so one screen
	// pixel is this step in normal-xy
	float step = 1.0f / RETRO_PROJECTION_SCALE;

	for (int y = 0; y < CHROME_SIZE; y++) {
		// -Ny for the row. A front face reads the lower rows at Ny > 0 (down);
		// ry < 0 then paints those rows as the floor
		float py = -(y - c) / c;
		for (int x = 0; x < CHROME_SIZE; x++) {
			float px = (x - c) / c;
			float r2 = px * px + py * py;
			float qx = px;
			float qy = py;
			if (r2 > 1.0f) {
				float s = 0.7071f / sqrt(r2);
				qx = px * s;
				qy = py * s;
				r2 = qx * qx + qy * qy;
			}

			unsigned char index = 0;
			if (r2 <= 1.0f) {
				float nz = sqrt(1.0f - r2);
				float ry = 2.0f * nz * qy;

				if (ry >= 0.0f) {
					// sqrt: a linear ramp of Ry is horizon-white over most of the cap
					int shade = (int)(sqrt(ry) * CHROME_SKY_SHADES + 0.5f);
					index = CHROME_SKY_START + CLAMP(shade, 0, CHROME_SKY_SHADES);
				} else {
					// A ray this close to horizontal, or one whose neighbour falls
					// off the floor, covers more than a square. The sample is then
					// the average of the checker, and the sky begins at Ry = 0.
					float fx, fz, fx1, fz1, fx2, fz2;
					float shown = 0.5f;
					if (ChromeFloorTile(qx, qy, fx, fz) && ChromeFloorTile(qx + step, qy, fx1, fz1) && ChromeFloorTile(qx, qy + step, fx2, fz2)) {
						float wx = MAX(fabs(fx1 - fx), fabs(fx2 - fx));
						float wz = MAX(fabs(fz1 - fz), fabs(fz2 - fz));
						if (MAX(wx, wz) < 8.0f) {
							shown = ChromeChecker(fx, fz, wx, wz);
							shown = MAX(0.0f, MIN(1.0f, shown));
						}
					}
					// Haze: the floor fades into the horizon colour with the
					// distance along the ray, so the squares too small to
					// resolve end in the sky's first entry, not in a band of
					// their average gray under it. See environcube.cpp, whose
					// flat faces stretch that band too wide to haze over this
					// way alone.
					float distance = CHROME_EYE_HEIGHT / -ry;
					float fog = distance / CHROME_HAZE_DISTANCE;
					float haze = 1.0f - exp(-fog * fog);
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
	// A mirror sphere spinning about its own centre is a no-op: every rotation
	// maps the ball onto itself, so the set of normals behind any fixed screen
	// pixel never changes and the reflection sits dead still. Orbiting the
	// ball across the floor instead moves the one thing that does change the
	// picture, where on the screen the reflection is centred.
	float phase = fmod(time * ORBIT_SPEED, 2 * M_PI);
	float x = RETRO_WIDTH / 2.0 + ORBIT_RADIUS_X * cos(phase);
	float y = RETRO_HEIGHT / 2.0 + ORBIT_RADIUS_Y * sin(phase);

	// Draw sphere
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, x, y);
	RETRO_RenderModel(RETRO_POLY_ENVIRONMENT);
}

void DEMO_Initialize(void)
{
	// Init palette: a stark checker so the ball reads as a mirror rather than
	// a lit surface, and a sky ramp from a pale horizon to a deep zenith
	RETRO_Palette palette[RETRO_COLORS];
	memset(palette, 0, sizeof(palette));
	RETRO_SetColor(CHROME_BG, RETRO_BLACK, palette);
	// Inclusive ends, so a half-covered square is the middle entry and a full
	// white square is white. Each haze level is that ramp mixed further into
	// the horizon, the last one entirely, and the sky ramp starts at the next
	// index with the same colour, so the floor meets the sky without a seam.
	for (int j = 0; j < CHROME_HAZE_LEVELS; j++) {
		float h = (float)j / (CHROME_HAZE_LEVELS - 1);
		for (int i = 0; i < CHROME_FLOOR_SHADES; i++) {
			float k = (float)i / (CHROME_FLOOR_SHADES - 1);
			RETRO_Palette color;
			color.r = RETRO_WHITE.r * k;
			color.g = RETRO_WHITE.g * k;
			color.b = RETRO_WHITE.b * k;
			color.r = color.r + (CHROME_HORIZON.r - color.r) * h;
			color.g = color.g + (CHROME_HORIZON.g - color.g) * h;
			color.b = color.b + (CHROME_HORIZON.b - color.b) * h;
			RETRO_SetColor(CHROME_FLOOR_START + j * CHROME_FLOOR_SHADES + i, color, palette);
		}
	}
	RETRO_CreateGradientPalette(CHROME_SKY_START, CHROME_SKY_START + CHROME_SKY_SHADES, CHROME_HORIZON, RETRO_MIDNIGHTBLUE, palette);
	RETRO_SetPalette(palette);

	// Build the reflection map once; nothing about it depends on the ball's orientation
	BuildChromeMap();

	Model3D *model = RETRO_Load3DModel("assets/spherequads.obj");
	model->envmap = ChromeMap;
	model->envmapwidth = CHROME_SIZE;
	model->envmapheight = CHROME_SIZE;

	// Rotated once, at rest: RETRO_ProjectModel reads rpos, which only
	// RETRO_RotateModel fills in, but a mirror sphere never needs to turn
	RETRO_RotateModel(0, 0, 0);
}
