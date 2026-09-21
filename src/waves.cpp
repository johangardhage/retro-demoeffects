//
// Waves
//
// A multi-layered ocean wave demo effect featuring parallax sine wave layers.
// "RETRO" floats in the back wave layer, while "DEMOEFFECTS" floats in the middle wave.
//
// Each letter is drawn before its own wave, so that wave's surface cuts it at the
// waterline. Each letter chases the surface under its center on a damped spring, so it
// lags and overshoots like something afloat.
//
// The layers are spaced so the next one never reaches the text: the middle wave stays
// far enough below the back wave to clear "DEMOEFFECTS" above its waterline, and the
// front wave never rises over the middle one. Worst case is both waves' sines at their
// extremes at once:
//
//   middle.base - back.base   - (back amplitudes + middle amplitudes) = 137 - 85 - 33 = 19
//   front.base  - middle.base - (middle amplitudes + front amplitudes) = 178 - 137 - 40 = 1
//
// "DEMOEFFECTS" shows at most FLOAT_DRAFT rows above its surface, plus however far its
// spring lags, which stays under 5 pixels, so 19 leaves it clear of "RETRO"'s waterline.
//
// Distance is carried by the palette: each layer is the water color hazed toward the
// horizon by how far away it is, and the sky darkens from the horizon upward. A lighter
// foam line tops every layer.
//
// Inspired by retro Amiga/VGA demo effects.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"
#include <ctype.h>

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }

// The font row that sits on the waterline. Rows above it show, rows from it down are under water
#define FLOAT_DRAFT 12

// Spring pulling a letter to the surface, a = stiffness * (surface - y) - damping * v.
// Stiffness is the natural frequency squared (5 rad/s), damping is 2 * 0.3 * 5 for a light overshoot
#define FLOAT_STIFFNESS 25.0f
#define FLOAT_DAMPING 3.0f

// Sky gradient, SKY_SHADES entries from the top of the screen down to the horizon at SKY_HORIZON,
// the lowest row the back wave ever uncovers
#define SKY_SHADES 32
#define SKY_HORIZON 100

// Sky and water colors. Each layer is WATER mixed toward HORIZON by its haze
#define SKY_TOP RETRO_Palette{ 112, 126, 142 }
#define HORIZON RETRO_Palette{ 176, 186, 190 }
#define WATER RETRO_Palette{ 56, 110, 180 }
#define FOAM RETRO_Palette{ 236, 242, 245 }
#define HAZE_BACK 0.5f
#define HAZE_MID 0.25f
#define HAZE_FRONT 0.0f
#define FOAM_MIX 0.45f

// Palette color indices
enum {
	COLOR_WAVE_BACK,
	COLOR_WAVE_MID,
	COLOR_WAVE_FRONT,
	COLOR_FOAM_BACK,
	COLOR_FOAM_MID,
	COLOR_FOAM_FRONT,
	COLOR_TEXT_BODY,
	COLOR_TEXT_TOP,
	COLOR_SKY
};

static RETRO_Font Font;

// A wave layer: a base height plus two travelling sines, amplitude * sin(k * x + speed * time)
struct Wave {
	float base;
	float amplitude1, k1, speed1;
	float amplitude2, k2, speed2;
};

static const Wave WaveBack  = {  85.0f, 10.0f, 0.025f,  1.2f, 5.0f, 0.040f, -0.8f };
static const Wave WaveMid   = { 137.0f, 12.0f, 0.020f, -1.5f, 6.0f, 0.050f,  1.0f };
static const Wave WaveFront = { 178.0f, 14.0f, 0.018f,  1.8f, 8.0f, 0.035f, -1.3f };

// A letter's waterline height and vertical velocity on its spring
struct Buoy {
	float y;
	float velocity;
};

// A word afloat on a wave, each letter spacing pixels apart and swaying sideways by
// sway * sin(swayspeed * time + 0.5 * i)
struct Word {
	const char *text;
	int length;
	int spacing;
	float sway, swayspeed;
	const Wave &wave;
	unsigned char color;
	Buoy *buoys;
};

static const char TextTop[] = "RETRO";
static const char TextMain[] = "DEMOEFFECTS";

static Buoy BuoysTop[sizeof(TextTop) - 1];
static Buoy BuoysMain[sizeof(TextMain) - 1];

static const Word WordTop  = { TextTop,  sizeof(TextTop) - 1,  28, 2.0f, 0.8f, WaveBack, COLOR_TEXT_TOP,  BuoysTop };
static const Word WordMain = { TextMain, sizeof(TextMain) - 1, 24, 2.5f, 0.9f, WaveMid,  COLOR_TEXT_BODY, BuoysMain };

// Mix a toward b by t
static RETRO_Palette Mix(RETRO_Palette a, RETRO_Palette b, float t)
{
	return RETRO_Palette{
		(unsigned char)lroundf(a.r + (b.r - a.r) * t),
		(unsigned char)lroundf(a.g + (b.g - a.g) * t),
		(unsigned char)lroundf(a.b + (b.b - a.b) * t) };
}

// Fill a wave layer from its surface down, the surface row in foam
static void DrawLayer(const int *surface, unsigned char color, unsigned char foam)
{
	for (int x = 0; x < RETRO_WIDTH; x++) {
		RETRO_DrawVline(x, surface[x], RETRO_HEIGHT - 1, color);
		RETRO_DrawVline(x, surface[x], surface[x], foam);
	}
}

// Surface height of a wave layer at column x; the text samples the same function to ride it
static float WaveHeight(const Wave &wave, float x, float time)
{
	return wave.base + wave.amplitude1 * sinf(wave.k1 * x + time * wave.speed1)
	                 + wave.amplitude2 * sinf(wave.k2 * x + time * wave.speed2);
}

// Center column of letter i, the word centered on the screen
static float LetterX(const Word &word, int i, float time)
{
	int start = (RETRO_WIDTH - word.length * word.spacing) / 2 + word.spacing / 2;
	return start + i * word.spacing + word.sway * sinf(time * word.swayspeed + i * 0.5f);
}

// Draw a 16x16 character centered on column cx, its FLOAT_DRAFT row at height waterline
static void DrawFloatingChar(char character, float cx, float waterline, unsigned char color)
{
	unsigned char code = (unsigned char)toupper((unsigned char)character);
	int left = (int)lroundf(cx) - Font.width / 2;
	int top = (int)lroundf(waterline) - FLOAT_DRAFT;

	for (int y = 0; y < Font.height; y++) {
		int py = top + y;
		if (py < 0 || py >= RETRO_HEIGHT) continue;

		for (int x = 0; x < Font.width; x++) {
			int px = left + x;
			if (px >= 0 && px < RETRO_WIDTH) {
				if (RETRO_FontInk(Font, code, x, y)) {
					RETRO_PutPixel(px, py, color);
				}
			}
		}
	}
}

static void DrawWord(const Word &word, float time)
{
	for (int i = 0; i < word.length; i++) {
		DrawFloatingChar(word.text[i], LetterX(word, i, time), word.buoys[i].y, word.color);
	}
}

// Pull each letter toward the surface under it. Semi-implicit Euler: velocity first,
// then position from the new velocity, which stays stable at this stiffness and step
static void FloatWord(const Word &word, float time, float timestep)
{
	for (int i = 0; i < word.length; i++) {
		Buoy &buoy = word.buoys[i];
		float surface = WaveHeight(word.wave, LetterX(word, i, time), time);

		buoy.velocity += (FLOAT_STIFFNESS * (surface - buoy.y) - FLOAT_DAMPING * buoy.velocity) * timestep;
		buoy.y += buoy.velocity * timestep;
	}
}

// Rest each letter on the surface, still
static void LaunchWord(const Word &word, float time)
{
	for (int i = 0; i < word.length; i++) {
		word.buoys[i] = { WaveHeight(word.wave, LetterX(word, i, time), time), 0.0f };
	}
}

// The springs chase the surface as it is displayed, so a stall that the simulation
// cannot catch up on leaves the letters behind for a moment rather than for good
void DEMO_FixedUpdate(double timestep)
{
	FloatWord(WordTop, (float)RETRO.time, (float)timestep);
	FloatWord(WordMain, (float)RETRO.time, (float)timestep);
}

void DEMO_Render(double time, double deltatime)
{
	// 1. Draw sky, darkening from the horizon upward. Rows below the horizon are all water
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int shade = MIN(y * SKY_SHADES / SKY_HORIZON, SKY_SHADES - 1);
		RETRO_DrawHline(0, RETRO_WIDTH - 1, y, COLOR_SKY + shade);
	}

	// Pre-calculate wave heights
	int wave1_y[RETRO_WIDTH];
	int wave2_y[RETRO_WIDTH];
	int wave3_y[RETRO_WIDTH];

	// Layer 1 (Back wave): slow, distant undulating curve
	// Layer 2 (Middle wave): medium speed wave
	// Layer 3 (Foreground wave): faster, rolling front water wave
	// RETRO_DrawVline clips, so the heights need no clamping
	for (int x = 0; x < RETRO_WIDTH; x++) {
		wave1_y[x] = (int)lroundf(WaveHeight(WaveBack, (float)x, (float)time));
		wave2_y[x] = (int)lroundf(WaveHeight(WaveMid, (float)x, (float)time));
		wave3_y[x] = (int)lroundf(WaveHeight(WaveFront, (float)x, (float)time));
	}

	// 2. Draw top text ("RETRO"), then Layer 1 (Back wave) over it, which sinks it to its waterline
	DrawWord(WordTop, (float)time);

	DrawLayer(wave1_y, COLOR_WAVE_BACK, COLOR_FOAM_BACK);

	// 3. Draw main text ("DEMOEFFECTS"), then Layer 2 (Middle wave) over it
	DrawWord(WordMain, (float)time);

	DrawLayer(wave2_y, COLOR_WAVE_MID, COLOR_FOAM_MID);

	// 4. Draw Layer 3 (Foreground wave), which never rises over Layer 2
	DrawLayer(wave3_y, COLOR_WAVE_FRONT, COLOR_FOAM_FRONT);
}

void DEMO_Initialize(void)
{
	// Set palette. The farther a layer, the more of the horizon it takes on
	RETRO_Palette back = Mix(WATER, HORIZON, HAZE_BACK);
	RETRO_Palette mid = Mix(WATER, HORIZON, HAZE_MID);
	RETRO_Palette front = Mix(WATER, HORIZON, HAZE_FRONT);

	RETRO_SetColor(COLOR_WAVE_BACK, back);
	RETRO_SetColor(COLOR_WAVE_MID, mid);
	RETRO_SetColor(COLOR_WAVE_FRONT, front);
	RETRO_SetColor(COLOR_FOAM_BACK, Mix(back, FOAM, FOAM_MIX));
	RETRO_SetColor(COLOR_FOAM_MID, Mix(mid, FOAM, FOAM_MIX));
	RETRO_SetColor(COLOR_FOAM_FRONT, Mix(front, FOAM, FOAM_MIX));
	RETRO_SetColor(COLOR_TEXT_BODY,    231, 228, 190); // Soft Pastel Yellow/Cream
	RETRO_SetColor(COLOR_TEXT_TOP,     244, 240, 208); // Cream White for RETRO
	RETRO_CreateGradientPalette(COLOR_SKY, COLOR_SKY + SKY_SHADES, SKY_TOP, HORIZON);

	// Load 16x16 font asset
	Font = RETRO_LoadFont(FONT);

	// Start the letters at rest on the surface
	LaunchWord(WordTop, (float)RETRO.time);
	LaunchWord(WordMain, (float)RETRO.time);
}
