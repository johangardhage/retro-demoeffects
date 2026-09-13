//
// Twister 6
//
// The flowers-wrapped square column from twister3, chrome-shaded and
// allowed to change thickness. twister3's torsion still winds the square
// up and down the period; on top of that the radius and the axis are waves
// in y, so the silhouette is an hourglass that snakes rather than a prism
// of constant width.
//
//   radius(y)   = R + WAVE · sin(phase_r + y · RADIUS_WAVE)
//   center_x(y) = CX + SWAY · sin(phase_s + y · SWAY_WAVE)
//   θ(y)        = (y · torsion(phase) + phase) · TURNS
//
// torsion is twister3's envelope, TORSION · sin(7 phase) · cos(phase). The
// four corners still ride a circle of that radius, and the two faces
// turned toward the viewer are the walk from the leftmost corner through
// the nearest to the rightmost. Each face carries a quarter of the flowers
// picture, squeezed to whatever width the face has on screen.
//
// Lighting is a cylinder, not a depth cue. Across a span the surface is
// treated as facing (x − cx) / radius. Shade is a mix of a broad phong
// (the roundness) and a tight one (the vertical stripe on the part of
// the column that points at the camera). The flowers picture owns every
// palette entry, so TwisterShadeTable maps each texel through that shade
// back into the picture's own colors; the top of the ramp walks partway
// toward white so the stripe reads as metal without erasing the picture.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retroshadetable.h"

#define TWISTER_PERIOD 512
#define TWISTER_CYCLE ((double)RETRO_ANGLES_PER_TURN / TWISTER_PERIOD)
#define TWISTER_CENTER_X (RETRO_WIDTH / 2.0)
#define TWISTER_RADIUS 70 // mean radius of the circle the four corners ride
#define TWISTER_RADIUS_WAVE 42 // how far the radius walks off that mean
#define TWISTER_SWAY 44 // how far the axis wanders off centre
#define TWISTER_TURNS 1.5
#define TWISTER_TORSION 1.2
#define TWISTER_TORSION_WAVE 7
#define TWISTER_RADIUS_Y 1.05 // radius cycles down the column, in turns
#define TWISTER_SWAY_Y 0.55 // and the axis, about half a turn so it is an S
#define TWISTER_RADIUS_PHASE 0.35 // how fast the radius wave rides the clock
#define TWISTER_SWAY_PHASE 0.7
#define TWISTER_SPEED 30.0
#define TWISTER_FORM 2.2f // broad cylindrical roundness
#define TWISTER_FALLOFF 16.0f // tight specular stripe down the facing side

#define TWISTER_IMAGE_SIZE 256
#define TWISTER_IMAGE_FACE (TWISTER_IMAGE_SIZE / 4)
#define TWISTER_IMAGE_SCROLL 2.0

#define TWISTER_SHADES 64
#define TWISTER_SPECULAR 10 // top of the ramp, walked partway toward white
#define TWISTER_DIFFUSE (TWISTER_SHADES - TWISTER_SPECULAR)
#define TWISTER_AMBIENT 0.32f
#define TWISTER_SPECULAR_MIX 0.50f // how far the highlight walks toward white

unsigned char TwisterShadeTable[RETRO_COLORS * TWISTER_SHADES];

//
// Map every source texel through a chrome ramp and back into the picture's
// own palette. The lower TWISTER_DIFFUSE entries run from ambient to the
// face colour; the remaining TWISTER_SPECULAR entries walk from that colour
// toward white, which is the metal highlight.
//
static void CreateChromeShadeTable(const RETRO_Palette *palette)
{
	for (int source = 0; source < RETRO_COLORS; source++) {
		RETRO_Palette color = palette[source];

		for (int shade = 0; shade < TWISTER_SHADES; shade++) {
			RETRO_Palette target;
			if (shade < TWISTER_DIFFUSE) {
				float level = TWISTER_DIFFUSE > 1 ? (float)shade / (TWISTER_DIFFUSE - 1) : 1.0f;
				float brightness = TWISTER_AMBIENT + (1.0f - TWISTER_AMBIENT) * level;
				target.r = (unsigned char)(color.r * brightness);
				target.g = (unsigned char)(color.g * brightness);
				target.b = (unsigned char)(color.b * brightness);
			} else {
				float t = (float)(shade - TWISTER_DIFFUSE + 1) / TWISTER_SPECULAR * TWISTER_SPECULAR_MIX;
				target.r = (unsigned char)(color.r + (255 - color.r) * t);
				target.g = (unsigned char)(color.g + (255 - color.g) * t);
				target.b = (unsigned char)(color.b + (255 - color.b) * t);
			}
			TwisterShadeTable[source * TWISTER_SHADES + shade] =
				RETRO_ClosestPaletteColor(target, palette, RETRO_COLORS);
		}
	}
}

//
// One scanline of one face, half-open in x. Texture u and the cylindrical
// normal both advance from where the unclipped span begins, so clipping
// the left edge keeps both lookups on the geometry rather than restarting
// them. nx = (x − cx) / radius is the front of a cylinder whose axis is
// this row's; shade mixes a broad phong of that normal with a tight one.
// A face turned edge on has right <= left and covers nothing.
//
static void DrawSpan(int left, int right, int y, unsigned char *texels, int base, double center_x, double radius)
{
	if (right <= left) {
		return;
	}

	float du = (float)TWISTER_IMAGE_FACE / (right - left);
	int x0 = MAX(left, 0);
	int x1 = MIN(right, RETRO_WIDTH);
	float u = base + (x0 - left) * du;
	float inv_radius = radius > 0.5 ? 1.0f / (float)radius : 0.0f;

	unsigned char *row = RETRO_FrameBuffer() + y * RETRO_WIDTH;
	for (int x = x0; x < x1; x++, u += du) {
		unsigned char texel = texels[(int)u];
		float nx = ((float)x - (float)center_x) * inv_radius;
		float nz_squared = 1.0f - nx * nx;
		float nz = nz_squared > 0.0f ? sqrtf(nz_squared) : 0.0f;
		float light = 0.62f * powf(nz, TWISTER_FORM) + 0.38f * powf(nz, TWISTER_FALLOFF);
		int shade = (int)(light * (TWISTER_SHADES - 1));
		row[x] = TwisterShadeTable[texel * TWISTER_SHADES + CLAMP(shade, 0, TWISTER_SHADES)];
	}
}

void DEMO_Render(double time, double deltatime)
{
	double phase = fmod(time * TWISTER_SPEED, TWISTER_PERIOD);
	double torsion = TWISTER_TORSION * SIN(phase * TWISTER_TORSION_WAVE * TWISTER_CYCLE) * COS(phase * TWISTER_CYCLE);
	double scroll = phase * TWISTER_IMAGE_SCROLL;
	double radius_clock = phase * TWISTER_RADIUS_PHASE;
	double sway_clock = phase * TWISTER_SWAY_PHASE;

	unsigned char *image = RETRO_ImageData();
	double radius_step = TWISTER_RADIUS_Y * RETRO_ANGLES_PER_TURN / RETRO_HEIGHT;
	double sway_step = TWISTER_SWAY_Y * RETRO_ANGLES_PER_TURN / RETRO_HEIGHT;

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double radius = TWISTER_RADIUS + TWISTER_RADIUS_WAVE * SIN(radius_clock + y * radius_step);
		double center_x = TWISTER_CENTER_X + TWISTER_SWAY * SIN(sway_clock + y * sway_step);
		if (radius < 8.0) {
			radius = 8.0;
		}

		double index = y * torsion + phase;
		int v = WRAP(y + scroll, TWISTER_IMAGE_SIZE);
		unsigned char *texels = image + v * TWISTER_IMAGE_SIZE;

		double angle = index * TWISTER_TURNS * TWISTER_CYCLE;
		double sin_radius = radius * SIN(angle);
		double cos_radius = radius * COS(angle);
		int corner_x[4] = {
			(int)lround(center_x - cos_radius),
			(int)lround(center_x + sin_radius),
			(int)lround(center_x + cos_radius),
			(int)lround(center_x - sin_radius),
		};

		int face = 0;
		for (int corner = 1; corner < 4; corner++) {
			if (corner_x[corner] < corner_x[face]) {
				face = corner;
			}
		}

		DrawSpan(corner_x[face], corner_x[(face + 1) & 3], y, texels, face * TWISTER_IMAGE_FACE, center_x, radius);
		DrawSpan(corner_x[(face + 1) & 3], corner_x[(face + 2) & 3], y, texels, ((face + 1) & 3) * TWISTER_IMAGE_FACE, center_x, radius);
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx", true);
	RETRO_Palette *palette = RETRO_ImagePalette();

	CreateChromeShadeTable(palette);
	RETRO_SetColor(0, RETRO_BLACK);
}
