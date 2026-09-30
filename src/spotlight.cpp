//
// Spotlight
//
// A picture in the dark, with LAMPS spotlights playing over it. The lamps hang
// LAMP_HEIGHT in front of the picture, each over a point of its own, and do not
// move. What moves is where each one is aimed, a point that rides a Lissajous
// figure over the picture, swinging across it xrate times and down it yrate
// times in AIM_PERIOD.
//
// A lamp throws a cone of light along the line to its aim. The light on a
// pixel depends on the ray from the lamp to it, and is the product of three
// things:
//
//   beam    full where the ray is within BEAM_INNER of the middle of the cone,
//           none where it is more than BEAM_OUTER from it, and falling
//           smoothly between the two
//   facing  the cosine of the angle the ray meets the picture at,
//           LAMP_HEIGHT / distance, since light that comes in at a slant is
//           spread over more of the picture
//   spread  (LAMP_HEIGHT / distance)², since the light thins out with the
//           square of the way it has gone
//
// so that the light is 1 in the middle of a beam aimed straight down. A cone
// cut by a plane is an ellipse: right under its lamp the pool of light is
// round, and the further from there it is aimed the longer it is drawn out,
// away from the lamp, and the fainter it gets.
//
// The light of the lamps is added together, to 1 at the most, and takes the
// picture from AMBIENT of its own colors up to all of them. The picture has a
// palette of its own with no darker copies of its colors in it, so the screen
// gets a palette fitted to the picture's colors at every one of SHADES levels
// of light, and a shade table holds the entry nearest every color at every
// level. The level a pixel is drawn at is ordered-dithered between neighbors,
// so that the steps from one to the next do not show as rings around a pool.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define LAMPS 2
#define LAMP_HEIGHT 260.0f // pixels from the picture out to a lamp
#define BEAM_INNER 0.11f // radians from the middle of a beam to where it starts to fade
#define BEAM_OUTER 0.22f // and to where it is gone
#define AIM_MARGIN 30 // pixels kept between an aim and the edge of the picture
#define AIM_PERIOD 30.0 // seconds for the aims to come back to where they began
#define AMBIENT 0.12f // light on the picture where no beam reaches
#define SHADES 64 // levels of light

struct Lamp {
	float x, y; // the point of the picture it hangs over
	int xrate, yrate; // swings of its aim across and down the picture in AIM_PERIOD
	float xphase; // radians the swing across is ahead of the swing down
};

static const Lamp Lamps[LAMPS] = {
	{ 70, 60, 2, 3, 0.4f },
	{ 250, 180, 3, 4, 2.0f },
};

// 4×4 ordered dither thresholds, (i + 0.5) / 16
static const float Bayer[4][4] = {
	{ 0.5f / 16, 8.5f / 16, 2.5f / 16, 10.5f / 16 },
	{ 12.5f / 16, 4.5f / 16, 14.5f / 16, 6.5f / 16 },
	{ 3.5f / 16, 11.5f / 16, 1.5f / 16, 9.5f / 16 },
	{ 15.5f / 16, 7.5f / 16, 13.5f / 16, 5.5f / 16 },
};

static RETRO_ColorHistogram Histogram;
static unsigned char ShadeTable[RETRO_COLORS * SHADES];

//
// A picture color in the light of the lamps, shade going from none of it to
// all of it
//
static RETRO_Palette Light(RETRO_Palette color, float shade, const float *tint)
{
	float brightness = AMBIENT + (1 - AMBIENT) * shade;
	return {
		(unsigned char)(color.r * brightness + 0.5f),
		(unsigned char)(color.g * brightness + 0.5f),
		(unsigned char)(color.b * brightness + 0.5f),
	};
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	float phase = fmod(time * 2 * M_PI / AIM_PERIOD, 2 * M_PI);

	// Aim the lamps
	vec3 axis[LAMPS];
	for (int i = 0; i < LAMPS; i++) {
		float aimx = RETRO_WIDTH / 2 + (RETRO_WIDTH / 2 - AIM_MARGIN) * cosf(Lamps[i].xrate * phase + Lamps[i].xphase);
		float aimy = RETRO_HEIGHT / 2 + (RETRO_HEIGHT / 2 - AIM_MARGIN) * cosf(Lamps[i].yrate * phase);

		axis[i] = normalize(vec3{ aimx - Lamps[i].x, aimy - Lamps[i].y, LAMP_HEIGHT });
	}

	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Draw picture, every pixel at the level the lamps light it to
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float light = 0;

			for (int i = 0; i < LAMPS; i++) {
				vec3 ray = { x + 0.5f - Lamps[i].x, y + 0.5f - Lamps[i].y, LAMP_HEIGHT };
				float distance = length(ray);
				float beam = smoothstep(cosf(BEAM_OUTER), cosf(BEAM_INNER), dot(ray, axis[i]) / distance);
				float facing = LAMP_HEIGHT / distance;

				light += beam * facing * facing * facing;
			}

			int shade = MIN((int)(CLAMP01(light) * (SHADES - 1) + Bayer[y & 3][x & 3]), SHADES - 1);
			buffer[y * RETRO_WIDTH + x] = ShadeTable[image[y * RETRO_WIDTH + x] * SHADES + shade];
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_320x240_quantizized.pcx");

	// Every level of light is as common as every other
	float lightweight[SHADES];
	for (float &weight : lightweight) {
		weight = 1;
	}

	// Init palette, fitted to the picture's colors at every level, and the
	// table that lights the picture's colors onto it
	RETRO_Palette palette[RETRO_COLORS];
	RETRO_ShadeTable shadetable = { ShadeTable, RETRO_COLORS, SHADES };
	RETRO_AddShadeTableColors(&Histogram, 0, shadetable, lightweight, Light);
	RETRO_CreateHistogramPalette(&Histogram, palette);
	RETRO_SetPalette(palette);
	RETRO_CreateShadeTable(RETRO_ImagePalette(), palette, shadetable, Light);
}
