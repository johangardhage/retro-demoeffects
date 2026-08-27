//
// Perspective ring scroller
//
// Characters are placed on tangent planes around a horizontal ring. Glyph i
// sits at ring angle phase + i 2pi/N, and a point (lx, ly) on its own plane is
// carried out to the ring by
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
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retrocolor.h"
#include "lib/retrogfx.h"
#include "lib/retromath.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define FONT_IMAGE_WIDTH 944
#define SCROLL_TEXT "    RETRO DEMOEFFECTS    "
#define SCROLL_LENGTH (sizeof(SCROLL_TEXT) - 1)

// Ring geometry and perspective projection.
#define RING_RADIUS 86.0
#define RING_Y 75.0
#define CAMERA_DISTANCE 260.0
#define PROJECTION_SCALE 1.53
#define RING_TILT 0.18
#define SCROLL_SPEED 0.72

// Each atlas character is reduced to a transparent one-bit glyph mask.
static unsigned char Glyph[SCROLL_LENGTH][FONT_HEIGHT][FONT_WIDTH];

// Increasing q values make later painter-ordered cells cover earlier cells.
static float PaintDepth;

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
static void DrawCell(double angle, double x, double y, int color)
{
	Vertex p[4] = { Project(angle, x, y), Project(angle, x + 1, y), Project(angle, x + 1, y + 1), Project(angle, x, y + 1) };
	PolygonPoint poly[4] = {};
	PaintDepth += 0.0001f;
	for (int i = 0; i < 4; i++) {
		poly[i].x = p[i].sx;
		poly[i].y = p[i].sy;
		poly[i].q = PaintDepth;
	}
	RETRO_DrawFlatPolygon(poly, 4, color);
}

static void DrawGlyph(int letter, double angle, bool shadow)
{
	// The far half uses muted colors while the near half uses bright colors.
	bool back = cos(angle) < 0;
	int color = back ? (shadow ? 4 : 3) : (shadow ? 5 : 6);
	double yoff = shadow ? 2.0 : 0.0;
	for (int y = 0; y < FONT_HEIGHT; y++)
		for (int x = 0; x < FONT_WIDTH; x++)
			if (Glyph[letter][y][x])
				DrawCell(angle, x - FONT_WIDTH / 2.0, y - FONT_HEIGHT / 2.0 + yoff, color);
}

static int BarEdge(int y, double phase)
{
	// Two low-frequency waves keep the dividing seam gently curved and moving.
	return RETRO_WIDTH / 2 + lround(4 * sin(phase + y * 0.018) + 2 * sin(phase * 0.6));
}

void DEMO_Render(double deltatime)
{
	static double phase = 0, barphase = 0;
	phase = fmod(phase + deltatime * SCROLL_SPEED, 2 * M_PI);
	barphase = fmod(barphase + deltatime * 0.8, 2 * M_PI);

	// Draw background
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int edge = BarEdge(y, barphase);
		RETRO_DrawLine(0, y, edge - 1, y, 1);
		RETRO_DrawLine(edge, y, RETRO_WIDTH - 1, y, 2);
	}

	// Draw scroller
	RETRO_ClearDepthBuffer();
	PaintDepth = 1.0f;
	int order[SCROLL_LENGTH];
	double angle[SCROLL_LENGTH];
	for (int i = 0; i < (int)SCROLL_LENGTH; i++) {
		order[i] = i;
		angle[i] = phase + i * (2 * M_PI / SCROLL_LENGTH);
	}
	for (int i = 1; i < (int)SCROLL_LENGTH; i++) {
		int item = order[i], j = i;
		while (j > 0 && cos(angle[order[j - 1]]) > cos(angle[item])) {
			order[j] = order[j - 1];
			j--;
		}
		order[j] = item;
	}
	for (int n = 0; n < (int)SCROLL_LENGTH; n++) {
		int i = order[n];
		DrawGlyph(i, angle[i], true);
		DrawGlyph(i, angle[i], false);
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
	RETRO_LoadImage("assets/font_16x16.pcx");
	unsigned char *font = RETRO_ImageData();
	for (int i = 0; i < (int)SCROLL_LENGTH; i++) {
		unsigned char c = SCROLL_TEXT[i];
		if (c < 32 || c >= 91) continue;
		unsigned char *src = font + (c - 32) * FONT_WIDTH;
		for (int y = 0; y < FONT_HEIGHT; y++)
			for (int x = 0; x < FONT_WIDTH; x++)
				Glyph[i][y][x] = src[y * FONT_IMAGE_WIDTH + x] != 0;
	}
}
