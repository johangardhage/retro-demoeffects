//
// Heat haze
//
// A road through a desert, seen through the air that shimmers over the hot
// ground. The scene is a still picture. Everything that moves is the air.
//
// Hot air is thinner than cool air and light is bent toward the thicker, so a
// line of sight that passes a place where the temperature changes across it
// comes out aimed a little to the cool side. What bends it is not how hot the
// air is but how fast that changes from one side of the line to the other, so
// the picture behind a field of temperature T is moved by the gradient of T.
// Pixel (x, y) is copied from
//
//   I(x + s ∂T/∂x,  y + s ∂T/∂y)
//
// A gradient has no curl, so the picture is squeezed and stretched and never
// swirled, which is what tells haze from a ripple on water.
//
// T is HAZE_WAVES sine waves added up, each with a direction and a phase of its
// own and a whole number of turns across HAZE_SIZE both ways:
//
//   T(x, y) = Σ sin(2π (kx x + ky y) / HAZE_SIZE + φ)
//
// so it repeats every HAZE_SIZE pixels with no seam, and its gradient is the
// same sum with a cosine and the wave numbers as weights. The wave numbers are
// picked between HAZE_KMIN and HAZE_KMAX turns, which sets how large the lumps
// of air are. The gradient is worked out once into a pair of tables and scaled
// so its length is 1 on the average.
//
// The air rises, and that is the tables read further down as time passes. Two
// readings are added, one rising faster than the other and drifting sideways,
// so the lumps do not go by as one sheet but change shape on the way up.
//
// The air that shimmers is the layer lying on the ground. A line of sight has
// most of it to pass where it runs far along the ground, which is at the
// horizon, and less the higher it looks into the sky or the nearer the ground
// it lands on. So s is HAZE_STRENGTH pixels on the horizon and falls to 1/e of
// it every HAZE_REACH rows away, up and down.
//
// The ground is a plane seen from one unit above it. The row d pixels below the
// horizon shows it at depth
//
//   z = FOCAL / d
//
// and a point x to the side of the middle and h high stands at
//
//   sx = W/2 + x d,  sy = HORIZON + d − h d
//
// which places the road, the lines painted on it and the poles beside it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define HAZE_SIZE 256 // pixels the field repeats after, both ways
#define HAZE_WAVES 24
#define HAZE_KMIN 8 // turns a wave makes across the field, at the least
#define HAZE_KMAX 16 // and at the most
#define HAZE_STRENGTH 1.2f // pixels a reading moves the picture on the horizon, on the average
#define HAZE_REACH 34.0f // rows from the horizon to where the haze is 1/e as strong
#define HAZE_RISE 34 // pixels a second the slow reading rises
#define HAZE_FASTRISE 61 // and the fast one
#define HAZE_DRIFT 9 // pixels a second the fast reading moves sideways

#define HORIZON 132 // first row of ground
#define FOCAL 240.0f // rows below the horizon times depth
#define ROAD_HALFWIDTH 1.25f // in eye heights, as every measure of the scene
#define EDGE_OFFSET 1.1f // from the middle of the road to an edge line
#define LINE_HALFWIDTH 0.035f
#define DASH_LENGTH 2.0f // of a dash of the middle line, and of the gap after it
#define POLES 14
#define POLE_SPACING 6.0f // depth from one pole to the next
#define POLE_OFFSET 2.3f // from the middle of the road
#define POLE_HEIGHT 4.6f
#define POLE_HALFWIDTH 0.06f
#define POLE_ARM 0.5f // half the length of the crossbar
#define POLE_ARMDROP 0.3f // from the top of the pole down to it
#define SUN_X 248
#define SUN_Y 34
#define SUN_RADIUS 13
#define STRATA 3 // rows a layer of rock is thick
#define BUTTE_SLOPE 5.0f // pixels a side takes to reach the top

// Palette entries, the first of each range
#define SKY_COLORS 64 // from the top of the screen down to the horizon
#define MESA_COLORS 4
#define SAND_COLORS 48 // from the horizon to the bottom of the screen
#define ROAD_COLORS 48
#define POLE_COLORS 16 // from the farthest pole to the nearest
#define SKY 0
#define MESA (SKY + SKY_COLORS)
#define SAND (MESA + MESA_COLORS)
#define ROAD (SAND + SAND_COLORS)
#define POLE (ROAD + ROAD_COLORS)
#define SUN (POLE + POLE_COLORS)
#define LINE (SUN + 1)

struct Butte {
	int x, halfwidth, height;
};

// The flat-topped hills on the horizon
static const Butte Buttes[] = {
	{ 22, 30, 21 }, { 70, 9, 30 }, { 96, 6, 26 }, { 214, 26, 34 }, { 252, 7, 24 }, { 300, 34, 17 },
};

static unsigned char Scene[RETRO_WIDTH * RETRO_HEIGHT];
static float ShiftX[HAZE_SIZE * HAZE_SIZE];
static float ShiftY[HAZE_SIZE * HAZE_SIZE];
static float Strength[RETRO_HEIGHT]; // pixels a reading moves each row

//
// Work out the gradient of the temperature
//
static void BuildHaze(void)
{
	float squares = 0;

	for (int i = 0; i < HAZE_WAVES; i++) {
		// Pick a wave whose number of turns is in range
		int kx, ky;
		do {
			kx = RANDOM(2 * HAZE_KMAX + 1) - HAZE_KMAX;
			ky = RANDOM(2 * HAZE_KMAX + 1) - HAZE_KMAX;
		} while (kx * kx + ky * ky < HAZE_KMIN * HAZE_KMIN || kx * kx + ky * ky > HAZE_KMAX * HAZE_KMAX);
		float offset = RANDOMF(2 * M_PI);

		for (int y = 0; y < HAZE_SIZE; y++) {
			for (int x = 0; x < HAZE_SIZE; x++) {
				float slope = cosf(2 * M_PI * (kx * x + ky * y) / HAZE_SIZE + offset);

				ShiftX[y * HAZE_SIZE + x] += kx * slope;
				ShiftY[y * HAZE_SIZE + x] += ky * slope;
			}
		}

		// A cosine squared is a half on the average
		squares += (kx * kx + ky * ky) * 0.5f;
	}

	for (int i = 0; i < HAZE_SIZE * HAZE_SIZE; i++) {
		ShiftX[i] /= sqrtf(squares);
		ShiftY[i] /= sqrtf(squares);
	}
}

//
// Height of the hills above the horizon at column x
//
static int Skyline(int x)
{
	float height = 3 + 2 * sinf(x * 0.045f) + sinf(x * 0.11f + 1.0f);

	for (const Butte &butte : Buttes) {
		height = MAX(height, butte.height * CLAMP01((butte.halfwidth - abs(x - butte.x)) / BUTTE_SLOPE));
	}
	return height;
}

static void DrawScene(void)
{
	// Draw sky and sun
	for (int y = 0; y < HORIZON; y++) {
		memset(Scene + y * RETRO_WIDTH, SKY + y * SKY_COLORS / HORIZON, RETRO_WIDTH);
	}
	RETRO_DrawEllipse(SUN_X, SUN_Y, SUN_RADIUS, SUN_RADIUS, SUN, {}, Scene);

	// Draw hills, in layers of rock
	for (int x = 0; x < RETRO_WIDTH; x++) {
		int height = Skyline(x);
		for (int y = HORIZON - height; y < HORIZON; y++) {
			Scene[y * RETRO_WIDTH + x] = MESA + (HORIZON - y) / STRATA % MESA_COLORS;
		}
	}

	// Draw ground: sand with a grain to it, the road, and the lines on the road
	for (int y = HORIZON; y < RETRO_HEIGHT; y++) {
		int d = y - HORIZON + 1;
		float z = FOCAL / d;
		int shade = (d - 1) * SAND_COLORS / (RETRO_HEIGHT - HORIZON);
		bool dash = fmodf(z, 2 * DASH_LENGTH) < DASH_LENGTH;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			float side = fabsf(x + 0.5f - RETRO_WIDTH / 2) / d;
			int grain = (int)(RETRO_Hash(x, y) % 5) - 2;

			if (side >= ROAD_HALFWIDTH) {
				Scene[y * RETRO_WIDTH + x] = SAND + CLAMP(shade + grain, 0, SAND_COLORS);
			} else if (fabsf(side - EDGE_OFFSET) < LINE_HALFWIDTH || (dash && side < LINE_HALFWIDTH)) {
				Scene[y * RETRO_WIDTH + x] = LINE;
			} else {
				Scene[y * RETRO_WIDTH + x] = ROAD + CLAMP(shade + grain / 2, 0, ROAD_COLORS);
			}
		}
	}

	// Draw poles from the farthest to the nearest, each with a crossbar and a
	// wire from either end of it to the pole behind
	int farleft = 0, farright = 0, fararm = 0;
	for (int i = POLES; i >= 1; i--) {
		float d = FOCAL / (i * POLE_SPACING);
		int x = RETRO_WIDTH / 2 - POLE_OFFSET * d;
		int base = HORIZON + d;
		int top = base - POLE_HEIGHT * d;
		int arm = top + POLE_ARMDROP * d;
		int left = x - POLE_ARM * d;
		int right = x + POLE_ARM * d;
		int halfwidth = POLE_HALFWIDTH * d;
		int color = POLE + (POLES - i) * POLE_COLORS / POLES;

		if (i < POLES) {
			RETRO_DrawLine(farleft, fararm, left, arm, color, {}, Scene);
			RETRO_DrawLine(farright, fararm, right, arm, color, {}, Scene);
		}
		RETRO_DrawRectangle(x - halfwidth, top, x + halfwidth, base, color, {}, Scene);
		RETRO_DrawRectangle(left, arm - halfwidth, right, arm + halfwidth, color, {}, Scene);

		farleft = left;
		farright = right;
		fararm = arm;
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Calculate phase: how far each reading of the field has risen
	int rise = (int)fmod(time * HAZE_RISE, HAZE_SIZE);
	int fastrise = (int)fmod(time * HAZE_FASTRISE, HAZE_SIZE);
	int drift = (int)fmod(time * HAZE_DRIFT, HAZE_SIZE);

	// Draw scene, each pixel copied from where the air has moved it to
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int slow = WRAP(y + rise, HAZE_SIZE) * HAZE_SIZE;
		int fast = WRAP(y + fastrise, HAZE_SIZE) * HAZE_SIZE;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			int slowx = WRAP(x, HAZE_SIZE);
			int fastx = WRAP(x + drift, HAZE_SIZE);
			int sourcex = CLAMP(x + (int)lroundf(Strength[y] * (ShiftX[slow + slowx] + ShiftX[fast + fastx])), 0, RETRO_WIDTH);
			int sourcey = CLAMP(y + (int)lroundf(Strength[y] * (ShiftY[slow + slowx] + ShiftY[fast + fastx])), 0, RETRO_HEIGHT);

			buffer[y * RETRO_WIDTH + x] = Scene[sourcey * RETRO_WIDTH + sourcex];
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The sky pales toward the horizon, and the sand, the road
	// and the poles pale toward it with distance
	RETRO_CreateGradientPalette(SKY, SKY + SKY_COLORS, { 44, 104, 196 }, { 240, 226, 196 });
	RETRO_CreateGradientPalette(MESA, MESA + MESA_COLORS, { 190, 122, 100 }, { 156, 88, 76 });
	RETRO_CreateGradientPalette(SAND, SAND + SAND_COLORS, { 232, 206, 160 }, { 204, 136, 66 });
	RETRO_CreateGradientPalette(ROAD, ROAD + ROAD_COLORS, { 178, 166, 150 }, { 62, 58, 62 });
	RETRO_CreateGradientPalette(POLE, POLE + POLE_COLORS, { 150, 126, 108 }, { 44, 30, 24 });
	RETRO_SetColor(SUN, RETRO_IVORY);
	RETRO_SetColor(LINE, RETRO_JASMINE);

	DrawScene();
	BuildHaze();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		Strength[y] = HAZE_STRENGTH * expf(-abs(y - HORIZON) / HAZE_REACH);
	}
}
