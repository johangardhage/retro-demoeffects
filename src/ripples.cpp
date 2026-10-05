//
// Ripples
//
// A reflection of the still picture in the water below WATER_YPOS (on this
// photo the hot-spring surface, the mirror plane), broken up by waves seen
// in perspective. The row d pixels below the plane mirrors the row d above
// it, and the water it shows lies at depth
//
//   z = WATER_EYE / d
//
// since rows closer to the plane look further out. The waves are a function
// of that depth, so they bunch up into the distance, and they tilt the
// reflected ray by the surface slope, which moves the sample by
//
//   ysrc = WATER_YPOS − d + d WATER_TILT sin(WATER_KZ z − WATER_SPEED t)
//
// The offset scales with d, so at the waterline the waves are too small to
// move a row and the reflection is still, while toward the bottom of the
// screen rows break up by up to WATER_TILT d. With these values the source
// stays above the plane, so it is always the picture and never the water.
//
// The water also gives back less light than the picture sends it, and less
// the nearer it is, so each reflected color is mixed toward the water's own
// dark blue, from WATER_MIX in the distance to WATER_MIX + WATER_MIXSTEP at
// the bottom of the screen. The picture has all 256 colors, so the mixes are
// a shade table of WATER_LEVELS levels, each mixed color remapped to the
// nearest one the picture has, and the level rises with d, ordered-dithered
// between neighbors so the steps do not show as bands.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retroshadetable.h"

#define WATER_YPOS 185 // mirror plane; the first water row is the one below
#define WATER_EYE 60.0 // scales rows to depth
#define WATER_TILT 0.1 // row offset per pixel below the plane
#define WATER_KZ 14.0 // wave number along the depth
#define WATER_SPEED 3.0 // radians a second
#define WATER_LEVELS 16 // shades of reflection, far to near
#define WATER_MIX 0.15f // how much water color the far level takes
#define WATER_MIXSTEP 0.35f // and the near level this much more

static const RETRO_Palette WaterColor = { 20, 34, 56 };

static unsigned char WaterTable[RETRO_COLORS * WATER_LEVELS];

//
// A picture color as the water reflects it, level going from far to near
//
static RETRO_Palette Water(RETRO_Palette color, float level, const float *tint)
{
	float blend = WATER_MIX + WATER_MIXSTEP * level;
	return mix(color, WaterColor, blend);
}

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * WATER_SPEED, 2 * M_PI);

	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Draw background
	RETRO_Blit(image);

	// Draw ripples, each row reading the picture at its rippled mirror row,
	// darkened toward the viewer
	for (int y = WATER_YPOS + 1; y < RETRO_HEIGHT; y++) {
		int d = y - WATER_YPOS;
		int ysrc = WATER_YPOS - d + (int)lround(d * WATER_TILT * sin(WATER_KZ * WATER_EYE / d - phase));
		float fade = (float)(d - 1) * (WATER_LEVELS - 1) / (RETRO_HEIGHT - WATER_YPOS - 1);

		for (int x = 0; x < RETRO_WIDTH; x++) {
			int level = MIN((int)(fade + RETRO_DitherThreshold(x, y)), WATER_LEVELS - 1);
			buffer[y * RETRO_WIDTH + x] = WaterTable[image[ysrc * RETRO_WIDTH + x] * WATER_LEVELS + level];
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx", true);

	// Init water table, every picture color at every level, remapped to the picture's own
	RETRO_ShadeTable watertable = { WaterTable, RETRO_COLORS, WATER_LEVELS };
	RETRO_CreateShadeTable(RETRO_ImagePalette(), RETRO_ImagePalette(), watertable, Water);
}
