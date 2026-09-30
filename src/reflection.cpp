//
// Water reflection
//
// A sunset over a lake: a sky, a sun that slowly sets and rises again, and two
// mountain ridges scrolling at different speeds, with the water below the
// horizon reflecting all of it through perspective waves.
//
// The scene above the horizon is drawn first, and each water row then reads
// it back. The row d pixels below the horizon mirrors the row d above, and
// the water it shows lies at depth
//
//   z = WATER_EYE / d
//
// since rows closer to the horizon look further out. The waves are functions
// of that depth, so they bunch up into the distance, and they tilt the
// reflected ray by the surface slope, which moves the sample by
//
//   dy = d WATER_TILT sin(WATER_KZ z − WATER_SPEED t)
//   dx = d WATER_SWAY sin(WATER_KX xw + WATER_KZ2 z + WATER_SWAYSPEED t),  xw = (x − W/2) / d
//
// xw being the pixel's sideways position on the water. Both offsets scale
// with d, so near the horizon the waves are too small to move a pixel and
// the reflection is almost still, while near the viewer rows break up.
//
// The scene takes the palette's first SCENE_COLORS entries and the water the
// same colors again, WATER_LEVELS times over, each mixed further toward the
// water's own color. The level rises with d in bands, so the reflection is
// brightest in the distance and darkens toward the viewer, the way water
// reflects more at a grazing angle.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define HORIZON 130 // first water row
#define SCENE_COLORS 64 // palette entries of the scene, repeated per water level
#define SKY_COLORS 40
#define SUN_COLORS 16
#define RIDGE_COLORS 4 // per ridge, shaded by height
#define SKY 0 // first entry of each range
#define SUN SKY_COLORS
#define FAR_RIDGE (SUN + SUN_COLORS)
#define NEAR_RIDGE (FAR_RIDGE + RIDGE_COLORS)
#define SUN_X 212
#define SUN_Y 70 // the sun's centre, halfway through its swing
#define SUN_RADIUS 22
#define SUN_SWING 60 // pixels the sun sinks and rises either side of SUN_Y
#define SUN_SPEED 0.15 // radians a second through the swing
#define RIDGE_PERIOD 640 // pixels before a ridge repeats, a whole number of each of its waves
#define FAR_SPEED 6 // pixels a second
#define NEAR_SPEED 16
#define WATER_LEVELS 3 // shades of reflection, far to near
#define WATER_MIX 0.3 // how much water color the far level takes
#define WATER_MIXSTEP 0.15 // and each nearer level more
#define WATER_EYE 80.0f // scales rows to depth
#define WATER_TILT 0.08f // row offset per pixel below the horizon
#define WATER_SWAY 0.05f // column offset per pixel below the horizon
#define WATER_KZ 12.0f // wave number along the depth
#define WATER_KZ2 8.0f
#define WATER_KX 24.0f // wave number across
#define WATER_SPEED 2.3f // radians a second
#define WATER_SWAYSPEED 1.7f

static const RETRO_Palette WaterColor = { 8, 24, 52 };

//
// Height of a ridge above the horizon at column x, scrolled by offset
//
static int Ridge(int x, double offset, float base, float amplitude, int wave1, int wave2, int wave3)
{
	float u = (float)(2 * M_PI * fmod(x + offset, RIDGE_PERIOD) / RIDGE_PERIOD);
	return base + amplitude * (0.55f * sinf(wave1 * u) + 0.3f * sinf(wave2 * u + 1.3f) + 0.15f * sinf(wave3 * u + 0.4f));
}

//
// Fill a ridge from its top down to the horizon, darker toward the water
//
static void DrawRidge(unsigned char *buffer, double offset, float base, float amplitude, int wave1, int wave2, int wave3, int color)
{
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int height = Ridge(x, offset, base, amplitude, wave1, wave2, wave3);
		for (int y = MAX(HORIZON - height, 0); y < HORIZON; y++) {
			buffer[y * RETRO_WIDTH + x] = color + MIN((y - (HORIZON - height)) * RIDGE_COLORS / MAX(height, 1), RIDGE_COLORS - 1);
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Draw sky
	for (int y = 0; y < HORIZON; y++) {
		memset(buffer + y * RETRO_WIDTH, SKY + y * SKY_COLORS / HORIZON, RETRO_WIDTH);
	}

	// Draw sun, brightest in the middle
	int suny = SUN_Y + SUN_SWING * sin(fmod(time * SUN_SPEED, 2 * M_PI));
	for (int y = MAX(suny - SUN_RADIUS, 0); y < MIN(suny + SUN_RADIUS, HORIZON); y++) {
		for (int x = SUN_X - SUN_RADIUS; x < SUN_X + SUN_RADIUS; x++) {
			float r = sqrtf((float)((x - SUN_X) * (x - SUN_X) + (y - suny) * (y - suny))) / SUN_RADIUS;
			if (r < 1.0f) {
				buffer[y * RETRO_WIDTH + x] = SUN + (int)(r * r * SUN_COLORS);
			}
		}
	}

	// Draw ridges, far then near
	DrawRidge(buffer, fmod(time * FAR_SPEED, RIDGE_PERIOD), 26, 18, 2, 5, 11, FAR_RIDGE);
	DrawRidge(buffer, fmod(time * NEAR_SPEED, RIDGE_PERIOD), 12, 12, 3, 7, 17, NEAR_RIDGE);

	// Draw water, each pixel reading the scene at its rippled mirror position
	float t = (float)fmod(time, 1000 * 2 * M_PI);
	for (int y = HORIZON; y < RETRO_HEIGHT; y++) {
		int d = y - HORIZON + 1;
		float z = WATER_EYE / d;
		int ysrc = CLAMP(HORIZON - d + (int)lroundf(d * WATER_TILT * sinf(WATER_KZ * z - WATER_SPEED * t)), 0, HORIZON);
		int level = 1 + (d - 1) * WATER_LEVELS / (RETRO_HEIGHT - HORIZON);
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float xw = (float)(x - RETRO_WIDTH / 2) / d;
			int xsrc = CLAMP(x + (int)lroundf(d * WATER_SWAY * sinf(WATER_KX * xw + WATER_KZ2 * z + WATER_SWAYSPEED * t)), 0, RETRO_WIDTH);
			buffer[y * RETRO_WIDTH + x] = buffer[ysrc * RETRO_WIDTH + xsrc] + level * SCENE_COLORS;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The scene first: sky from night blue through purple to
	// orange at the horizon, the sun white to orange, two ridges of
	// silhouette, the far one hazed toward the sky
	RETRO_CreateGradientPalette(SKY, SKY + SKY_COLORS / 2, { 16, 20, 72 }, { 120, 48, 128 });
	RETRO_CreateGradientPalette(SKY + SKY_COLORS / 2, SKY + SKY_COLORS, { 120, 48, 128 }, { 255, 150, 60 });
	RETRO_CreateGradientPalette(SUN, SUN + SUN_COLORS, { 255, 255, 230 }, { 255, 170, 40 });
	RETRO_CreateGradientPalette(FAR_RIDGE, FAR_RIDGE + RIDGE_COLORS, { 92, 44, 100 }, { 70, 32, 82 });
	RETRO_CreateGradientPalette(NEAR_RIDGE, NEAR_RIDGE + RIDGE_COLORS, { 36, 16, 44 }, { 20, 8, 28 });

	// Then the water levels, the scene again mixed toward the water's color
	for (int level = 1; level <= WATER_LEVELS; level++) {
		float mix = WATER_MIX + WATER_MIXSTEP * (level - 1);
		for (int i = 0; i < SCENE_COLORS; i++) {
			RETRO_Palette color = RETRO_GetColor(i);
			color.r = color.r + (WaterColor.r - color.r) * mix;
			color.g = color.g + (WaterColor.g - color.g) * mix;
			color.b = color.b + (WaterColor.b - color.b) * mix;
			RETRO_SetColor(level * SCENE_COLORS + i, color);
		}
	}
}
