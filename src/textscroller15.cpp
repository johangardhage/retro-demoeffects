//
// Upward scroller on a twisting vertical ribbon. Each horizontal band of
// a generated text strip rotates about the same axis; varying the angle
// with height bends the letters as they turn. Chrome is a mirror: the view
// ray reflected about the twisted surface normal, on both faces alike.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropoly.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
static const char ScrollText[] = "RETRO DEMOEFFECTS...";
#define LETTERS (int)(sizeof(ScrollText) - 1)
#define LETTER_WIDTH 52.0f
#define LETTER_HEIGHT 48.0f
#define LINE_SPACING 60.0
#define SCROLL_SPEED 115.0
#define ROTATION_SPEED -1.8
#define TWIST_RATE 0.026
#define AXIS_X (RETRO_WIDTH * 0.78f)
#define CAMERA_DISTANCE 700.0f
#define SHADES 64
#define INK_BASE 16
// A letter is drawn from when its top is CULL_MARGIN below the screen until
// its bottom is CULL_MARGIN above it; the pass ends when the last one leaves.
#define CULL_MARGIN 16
#define ENTRY_Y (RETRO_HEIGHT + CULL_MARGIN)
#define PASS_GAP 1.0
#define PASS_CYCLE ((ENTRY_Y + (LETTERS - 1) * LINE_SPACING + LETTER_HEIGHT + CULL_MARGIN) / SCROLL_SPEED + PASS_GAP)

static RETRO_Font Font;
static RETRO_Image *TextStrip;
static unsigned char Background[RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char InkShades[2 * SHADES];
static ShadeTable TextShades = { InkShades, 2, SHADES };

static PolygonPoint Project(float x, float y, vec2 uv, double time)
{
	double angle = (y - RETRO_HEIGHT / 2.0) * TWIST_RATE + time * ROTATION_SPEED;
	float c = (float)cos(angle), s = (float)sin(angle);
	Vertex vertex = {};
	// Rotation about the vertical axis leaves height unchanged in 3D.
	vertex.rpos = { x * c, y - RETRO_HEIGHT / 2.0f, -x * s };
	RETRO_ProjectVertex(&vertex, 1.0f, AXIS_X, RETRO_HEIGHT / 2.0f, CAMERA_DISTANCE);

	// The ribbon's normal, turned with it and tilted by the twist: the
	// inverse-transpose of the twist's local derivative.
	vec3 n = { -s, 0, -c };
	n.y += (float)TWIST_RATE * (vertex.rpos.x * n.z - vertex.rpos.z * n.x);
	n = normalize(n);

	// Mirror chrome: the view ray from the eye, reflected about the normal,
	// looks up an environment that varies around the vertical axis and runs
	// from a bright sky above to a dark ground below. Reflection does not
	// depend on the normal's sign, so both faces shine alike.
	vec3 r = reflect(normalize(vertex.rpos - vec3{ 0, 0, -CAMERA_DISTANCE }), n);
	float around = (float)(0.5 - 0.5 * sin(atan2(r.x, r.z) + 0.6));
	float light = CLAMP01(around + (0.5f * (1.0f - r.y) - around) * fabs(r.y));
	return { vertex.spos, light * (SHADES - 1), uv, vertex.q };
}

void DEMO_Render(double time, double deltatime)
{
	memcpy(RETRO_FrameBuffer(), Background, sizeof(Background));
	RETRO_ClearDepthBuffer();
	double phase = fmod(time, PASS_CYCLE) * SCROLL_SPEED;
	for (int i = 0; i < LETTERS; i++) {
		float top = (float)(ENTRY_Y + i * LINE_SPACING - phase);
		if (top > ENTRY_Y || top + LETTER_HEIGHT < -CULL_MARGIN || ScrollText[i] == ' ') continue;
		// One band per world-space pixel gives a continuous twist instead
		// of rotating each character as a rigid card.
		for (int row = 0; row < (int)LETTER_HEIGHT; row++) {
			float y = top + row;
			float v0 = i * Font.height + row * Font.height / LETTER_HEIGHT;
			float v1 = v0 + Font.height / LETTER_HEIGHT;
			PolygonPoint quad[4] = {
				Project(-LETTER_WIDTH / 2, y, { 0, v0 }, time),
				Project(LETTER_WIDTH / 2, y, { (float)TextStrip->width, v0 }, time),
				Project(LETTER_WIDTH / 2, y + 1, { (float)TextStrip->width, v1 }, time),
				Project(-LETTER_WIDTH / 2, y + 1, { 0, v1 }, time)
			};
			RETRO_DrawTexMapGouraudPolygon(quad, 4, TextStrip->data,
				TextStrip->width, TextStrip->height, TextShades);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_Palette{ 0, 0, 0 });
	RETRO_SetColor(1, RETRO_Palette{ 68, 82, 118 });
	RETRO_SetColor(2, RETRO_Palette{ 33, 49, 84 });
	const RETRO_Palette chrome[] = { { 63, 27, 54 }, { 93, 91, 124 },
		{ 118, 158, 218 }, { 173, 216, 248 }, { 247, 255, 255 } };
	for (int i = 0; i < SHADES; i++) {
		float position = (float)i * 4 / (SHADES - 1);
		int band = MIN((int)position, 3);
		float blend = position - band;
		RETRO_Palette a = chrome[band], b = chrome[band + 1];
		RETRO_SetColor(INK_BASE + i, RETRO_Palette{
			(unsigned char)(a.r + (b.r - a.r) * blend),
			(unsigned char)(a.g + (b.g - a.g) * blend),
			(unsigned char)(a.b + (b.b - a.b) * blend) });
		InkShades[SHADES + i] = (unsigned char)(INK_BASE + i);
	}

	// Each character becomes a separate centered row in the vertical strip.
	char characters[LETTERS][2] = {};
	const char *lines[LETTERS];
	for (int i = 0; i < LETTERS; i++) {
		characters[i][0] = ScrollText[i];
		lines[i] = characters[i];
	}
	Font = RETRO_LoadFont(FONT);
	TextStrip = RETRO_GenerateTextImage(Font, lines, LETTERS);
	for (int i = 0; i < TextStrip->width * TextStrip->height; i++)
		TextStrip->data[i] = TextStrip->data[i] != 0;

	const vec2 edge[] = { { 160, 24 }, { 188, 51 }, { 174, 66 },
		{ 202, 91 }, { 160, 133 }, { 188, 161 }, { 174, 176 },
		{ 188, 190 }, { 160, 216 } };
	for (int segment = 0; segment < 8; segment++) {
		vec2 a = edge[segment], b = edge[segment + 1];
		for (int y = (int)a.y; y < (int)b.y; y++) {
			int right = (int)(a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y));
			for (int x = 0; x < right; x++)
				Background[y * RETRO_WIDTH + x] = y < 133 ? 1 : 2;
		}
	}
}
