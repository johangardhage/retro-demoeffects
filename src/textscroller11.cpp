//
// Scroller, perspective ring
//
// Each glyph is a stack of tiny quads on a tangent plane around a
// horizontal ring, so a letter bends with the ring instead of stamping
// flat onto it. Glyph pixels are read straight out of RETRO_LoadFont's
// atlas at render time.
//
// Glyph i sits at ring angle phase + column[i] * angle-per-pixel, where
// column[i] is its running pixel offset into the flattened text. Angle-per-
// pixel is 2π / (n · WRAP_WIDTH) so n glyphs of that width wrap the ring
// once. Glyphs narrower than WRAP_WIDTH keep their true spacing instead of
// being stretched to fill the circle; a shorter total run just leaves the
// rest of the ring bare. A point (lx, ly) on its own plane is carried out
// to the ring by
//
//   p = Ry(-a) (lx, ly, -RING_RADIUS)
//
// so local x runs along the tangent and local y down the character, and the
// glyph faces outward wherever it stands. The ring is then tilted about x and
// projected, so its near side appears larger and lower than its far side.
//
// A glyph is drawn a font pixel at a time, each one projected as its own
// quadrilateral rather than as a texture: the four corners are four calls to
// the same transform, which is what lets a character bend around the ring
// instead of being a flat stamp on it.
//
// cos(a) is the whole of the depth sort. rz comes out as lx sin a - R cos a,
// and lx is under half a glyph where R is 86, so the sign of cos a orders the
// ring on its own. The far half takes muted colors and the near half bright
// ones, which is the only fog here. Cells are then painted in that order with
// a q that only ever increases, so a later cell covers an earlier one and the
// depth buffer enforces the painter's order the sort chose. That q is a paint
// counter, not a reciprocal depth; nothing here interpolates it.
//
// A moving multicolor seam divides the background and passes in front of the
// text, since it is drawn last.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retropalette.h"
#include "lib/retrogfx.h"
#include "lib/retromath.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8
static const char *const ScrollText[] = { "    RETRO DEMOEFFECTS..." };

// Ring geometry and perspective projection.
#define RING_RADIUS 86.0
#define RING_Y 75.0
#define CAMERA_DISTANCE 260.0
#define PROJECTION_SCALE 1.53
#define RING_TILT 0.18
#define SCROLL_SPEED 0.72
#define WRAP_WIDTH 16.0

static RETRO_Font Font;

// Transform a point from a character's tangent plane into screen space.
// Local x follows the ring tangent and local y runs down the character.
static Vertex Project(double angle, double localx, double localy)
{
	Vertex vertex = {};
	vertex.x = localx;
	vertex.y = localy;
	vertex.z = -RING_RADIUS;

	// Rotate the local point around the ring, tilt the ring, and project it.
	RETRO_RotateVertex(&vertex, 0, -angle, 0);
	vertex.x = vertex.rx;
	vertex.y = vertex.ry;
	vertex.z = vertex.rz;
	RETRO_RotateVertex(&vertex, RING_TILT, 0, 0);
	RETRO_ProjectVertex(&vertex, PROJECTION_SCALE, RETRO_WIDTH / 2, RING_Y, CAMERA_DISTANCE);
	return vertex;
}

// Project and fill one source-font pixel as a screen-space quadrilateral.
// Increasing paintdepth makes each later cell cover the ones before it.
static void DrawCell(double angle, double x, double y, int color, float &paintdepth)
{
	Vertex p[4] = { Project(angle, x, y), Project(angle, x + 1, y), Project(angle, x + 1, y + 1), Project(angle, x, y + 1) };
	PolygonPoint poly[4] = {};
	paintdepth += 0.0001f;
	for (int i = 0; i < 4; i++) {
		poly[i].x = p[i].sx;
		poly[i].y = p[i].sy;
		poly[i].q = paintdepth;
	}
	RETRO_DrawFlatPolygon(poly, 4, color);
}

static void DrawGlyph(int letter, double angle, bool shadow, float &paintdepth)
{
	// The far half uses muted colors while the near half uses bright colors.
	bool back = cos(angle) < 0;
	int color = back ? (shadow ? 4 : 3) : (shadow ? 5 : 6);
	double yoff = shadow ? Font.height / 8.0 : 0.0;
	unsigned char code = (unsigned char)ScrollText[0][letter];
	int width = RETRO_CharWidth(Font, code);
	int glyph = code - Font.firstcharacter;
	int sourcex = glyph * Font.width;
	int copywidth = MIN(Font.width, width);
	if (glyph < 0 || sourcex + Font.width > Font.atlas->width) {
		return;
	}
	for (int y = 0; y < Font.height; y++)
		for (int x = 0; x < copywidth; x++)
			if (Font.atlas->data[y * Font.atlas->width + sourcex + x])
				DrawCell(angle, x - width / 2.0, y - Font.height / 2.0 + yoff, color, paintdepth);
}

static int BarEdge(int y, double phase)
{
	// Two low-frequency waves keep the dividing seam gently curved and moving.
	return RETRO_WIDTH / 2 + lround(4 * sin(phase + y * 0.018) + 2 * sin(phase * 0.6));
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, 2 * M_PI);
	double barphase = fmod(time * 0.8, 2 * M_PI);

	// Draw background
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int edge = BarEdge(y, barphase);
		RETRO_DrawLine(0, y, edge - 1, y, 1);
		RETRO_DrawLine(edge, y, RETRO_WIDTH - 1, y, 2);
	}

	// Draw scroller
	RETRO_ClearDepthBuffer();
	float paintdepth = 1.0f;
	int length = (int)strlen(ScrollText[0]);
	double angleperpixel = 2 * M_PI / (length * WRAP_WIDTH);
	int order[256];
	double angle[256];
	int cursor = 0;
	for (int i = 0; i < length; i++) {
		order[i] = i;
		int width = RETRO_CharWidth(Font, (unsigned char)ScrollText[0][i]);
		angle[i] = phase + (cursor + width / 2.0) * angleperpixel;
		cursor += width;
	}
	for (int i = 1; i < length; i++) {
		int item = order[i], j = i;
		while (j > 0 && cos(angle[order[j - 1]]) > cos(angle[item])) {
			order[j] = order[j - 1];
			j--;
		}
		order[j] = item;
	}
	for (int n = 0; n < length; n++) {
		int i = order[n];
		DrawGlyph(i, angle[i], true, paintdepth);
		DrawGlyph(i, angle[i], false, paintdepth);
	}

	// Draw neon seam
	static const int offsets[] = { -5, -3, -1, 1, 3 };
	static const int colors[] = { 7, 8, 9, 10, 11 };
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int edge = BarEdge(y, barphase);
		for (int i = 0; i < 5; i++)
			RETRO_DrawLine(edge + offsets[i], y, edge + offsets[i] + 1, y, colors[i]);
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(1, RETRO_DARKMIDNIGHTBLUE);
	RETRO_SetColor(2, RETRO_MUTEDINDIGO);
	RETRO_SetColor(3, RETRO_LIGHTSLATEGRAY);
	RETRO_SetColor(4, RETRO_MUTEDDARKSLATEBLUE);
	RETRO_SetColor(5, RETRO_PALESKYBLUE);
	RETRO_SetColor(6, RETRO_SOFTIVORY);
	RETRO_SetColor(7, RETRO_BRIGHTBLUE);
	RETRO_SetColor(8, RETRO_BRIGHTCYAN);
	RETRO_SetColor(9, RETRO_SOFTGHOSTWHITE);
	RETRO_SetColor(10, RETRO_LIGHTMAGENTA);
	RETRO_SetColor(11, RETRO_DEEPDARKVIOLET);

	// Init font
	Font = RETRO_LoadFont(FONT);
}
