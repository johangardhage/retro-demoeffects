//
// Upward scroller with solid, extruded letters. The generated text mask
// becomes front/back caps and exposed contour walls, including holes.
// Subdivided faces follow the vertical twist and share a real depth buffer.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropoly.h"
#include <vector>

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
static const char ScrollText[] = "RETRO DEMOEFFECTS...";
#define LETTERS (int)(sizeof(ScrollText) - 1)
#define LETTER_WIDTH 52.0f
#define LETTER_HEIGHT 48.0f
#define LETTER_DEPTH 10.0f
#define ROW_SEGMENTS 3 // keep the twist smooth between font rows
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
struct GlyphFace {
	vec3 corner[4];
	vec3 normal;
};
static std::vector<GlyphFace> Glyphs[LETTERS];

static void AddFace(int letter, vec3 a, vec3 b, vec3 c, vec3 d, vec3 normal)
{
	Glyphs[letter].push_back({ { a, b, c, d }, normal });
}

static bool Ink(int letter, int x, int y)
{
	if (x < 0 || x >= Font.width || y < 0 || y >= Font.height) return false;
	return TextStrip->data[(letter * Font.height + y) * TextStrip->width + x] != 0;
}

static void BuildGlyph(int letter)
{
	float dx = LETTER_WIDTH / Font.width, dy = LETTER_HEIGHT / Font.height;
	float front = -LETTER_DEPTH / 2, back = LETTER_DEPTH / 2;
	for (int y = 0; y < Font.height; y++) {
		// Merge horizontal ink runs into caps instead of making cubes for
		// every bitmap pixel. Internal walls would never be visible.
		for (int x = 0; x < Font.width;) {
			if (!Ink(letter, x, y)) { x++; continue; }
			int start = x;
			while (x < Font.width && Ink(letter, x, y)) x++;
			float left = start * dx - LETTER_WIDTH / 2;
			float right = x * dx - LETTER_WIDTH / 2;
			for (int row = 0; row < ROW_SEGMENTS; row++) {
				float top = (y + (float)row / ROW_SEGMENTS) * dy;
				float bottom = (y + (float)(row + 1) / ROW_SEGMENTS) * dy;
				AddFace(letter, { left, top, front }, { right, top, front },
					{ right, bottom, front }, { left, bottom, front }, { 0, 0, -1 });
				AddFace(letter, { left, top, back }, { right, top, back },
					{ right, bottom, back }, { left, bottom, back }, { 0, 0, 1 });
				AddFace(letter, { left, top, front }, { left, top, back },
					{ left, bottom, back }, { left, bottom, front }, { -1, 0, 0 });
				AddFace(letter, { right, top, front }, { right, top, back },
					{ right, bottom, back }, { right, bottom, front }, { 1, 0, 0 });
			}
		}
		for (int x = 0; x < Font.width; x++) {
			if (!Ink(letter, x, y)) continue;
			float left = x * dx - LETTER_WIDTH / 2, right = left + dx;
			float top = y * dy, bottom = top + dy;
			if (!Ink(letter, x, y - 1))
				AddFace(letter, { left, top, front }, { right, top, front },
					{ right, top, back }, { left, top, back }, { 0, -1, 0 });
			if (!Ink(letter, x, y + 1))
				AddFace(letter, { left, bottom, front }, { right, bottom, front },
					{ right, bottom, back }, { left, bottom, back }, { 0, 1, 0 });
		}
	}
}

static PolygonPoint Project(vec3 point, vec3 normal, float top, double time)
{
	float y = point.y + top - RETRO_HEIGHT / 2.0f;
	double angle = y * TWIST_RATE + time * ROTATION_SPEED;
	float c = (float)cos(angle), s = (float)sin(angle);
	Vertex vertex = {};
	vertex.rpos = { point.x * c + point.z * s, y, -point.x * s + point.z * c };
	RETRO_ProjectVertex(&vertex, 1.0f, AXIS_X, RETRO_HEIGHT / 2.0f, CAMERA_DISTANCE);

	// Inverse-transpose of the twist's local derivative: the surface
	// normal tilts with the deformation, not just with its Y rotation.
	vec3 n = { normal.x * c + normal.z * s, normal.y, -normal.x * s + normal.z * c };
	n.y += (float)TWIST_RATE * (vertex.rpos.x * n.z - vertex.rpos.z * n.x);
	n = normalize(n);

	// Mirror chrome: the view ray from the eye, reflected about the normal,
	// looks up an environment that varies around the vertical axis and runs
	// from a bright sky above to a dark ground below.
	vec3 r = reflect(normalize(vertex.rpos - vec3{ 0, 0, -CAMERA_DISTANCE }), n);
	float around = (float)(0.5 - 0.5 * sin(atan2(r.x, r.z) + 0.6));
	float light = CLAMP01(around + (0.5f * (1.0f - r.y) - around) * fabs(r.y));
	return { vertex.spos, INK_BASE + light * (SHADES - 1), {}, vertex.q };
}

void DEMO_Render(double time, double deltatime)
{
	memcpy(RETRO_FrameBuffer(), Background, sizeof(Background));
	RETRO_ClearDepthBuffer();
	double phase = fmod(time, PASS_CYCLE) * SCROLL_SPEED;
	for (int i = 0; i < LETTERS; i++) {
		float top = (float)(ENTRY_Y + i * LINE_SPACING - phase);
		if (top > ENTRY_Y || top + LETTER_HEIGHT < -CULL_MARGIN) continue;
		for (const GlyphFace &face : Glyphs[i]) {
			PolygonPoint quad[4];
			for (int j = 0; j < 4; j++)
				quad[j] = Project(face.corner[j], face.normal, top, time);
			RETRO_DrawGouraudPolygon(quad, 4);
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

	for (int i = 0; i < LETTERS; i++) BuildGlyph(i);

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
