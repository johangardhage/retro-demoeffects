//
// 3D lightsourced Glenz scroller
//
// The same extruded glyph helix as vectorscroller.cpp, drawn as lightsourced
// Glenz vectors: both sides of every face, additive palette indices, one
// Lambert term per face, the same shade as glenzshadedcube.cpp
//
//   shade = c + face->c + (N · L) · intensity
//
// Back faces use half the Lambert term. There is no ShadeFromLambert: the
// palette is a linear black–magenta gradient, and converting θ would
// bend that falloff. L is (0, 0, −1), from the eye. A letter that only
// banked with the helix would keep its front on the light, so each glyph
// also takes the cube's turn: a full spin about y and a slow elliptic
// rock about x and z, on top of the path tangent. A face writes one
// shades-wide Lambert range, so a front-lit stroke stays glass; past 3 ·
// shades the palette keeps going into white, so a genuine triple overlap
// reads as a highlight. A voxel grid would also stack three lit faces at
// every cube corner and speckle the glyph with that same wash, so
// coplanar ink is merged into larger quads first: the front of a stroke
// is one face, not one per pixel, and only the letter's own corners can
// triple. Buried walls between cubes are still omitted, or a stroke
// would pick up a third layer and blow out on its own. Model y is the
// screen's, growing down, and the front of a letter faces the eye at
// −z. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define FONT RETRO_FONT_VGA_8X8
//#define FONT RETRO_FONT_TOPAZ13_8X8
//#define FONT RETRO_FONT_TOPAZ30_8X8
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define LETTER_HEIGHT 1.55f // model units a glyph stands, top to bottom
#define LETTER_DEPTH 0.48f // model units a glyph is extruded along z
#define LETTER_GAP 0.50f // model units of extra advance after each glyph
#define SPACE_WIDTH 0.90f // model units a space occupies along the strip

#define SCROLL_SPEED 1.85f // model units a second along the helix
#define CULL 6.5f // |s| past which a letter is not drawn

#define HELIX_DISTANCE 2.35f // model units the helix stands behind the origin
#define HELIX_AMP_Y 0.72f // vertical radius of the helix
#define HELIX_AMP_Z 1.05f // depth radius of the helix
#define HELIX_WAVE 0.62f // radians of the helix per model unit of s
#define HELIX_SPIN 0.35f // radians a second the helix twists

#define LETTER_SPIN 1.1f // radians a second about y, so faces turn through the light
#define LETTER_ROCK 0.25f // radians the rock reaches about x and about z
#define LETTER_ROCK_SPEED 0.7f // radians a second around the rock

#define GLENZ_FACE 30 // added to a face before the Lambert term, as in glenzshadedcube.cpp
#define GLENZ_SHADES 64 // Lambert range of one face; the add fills at 3 · shades

#define MAX_SCROLL_LETTERS 256
#define MAX_GLYPH_SIZE 32 // atlas cell; a 16×16 font still fits the ink mask
#define MAX_DISTINCT_GLYPHS 32 // more than the alphabet any one ScrollText is likely to use

static const char ScrollText[] = "RETRO DEMOEFFECTS...           ";

static RETRO_Font Font;
static int TextLength;
static float LetterS[MAX_SCROLL_LETTERS];
static float TextWidth;
static float Pixel;

static Model3D *GlyphCache[MAX_DISTINCT_GLYPHS];
static unsigned char GlyphCacheChar[MAX_DISTINCT_GLYPHS];
static int GlyphCacheCount;

// Lay text out along the helix: letterS[i] is where letter i's cube starts,
// spaced by the font's fixed width plus gap, or spacewidth for a literal
// space. textWidth is one lap's length, the point at which the strip
// repeats. RageQuits if text holds more letters than letterS can take.
static int BuildLetterStrip(const char *text, const RETRO_Font &font, float pixel, float gap,
							 float spacewidth, float *letterS, int maxletters, float *textWidth)
{
	int length = (int)strlen(text);
	if (length > maxletters) {
		RETRO_RageQuit("Scroll text is longer than the letter list\n");
	}

	float s = 0;
	for (int i = 0; i < length; i++) {
		letterS[i] = s;
		if (text[i] == ' ') {
			s += spacewidth;
		} else {
			s += font.width * pixel + gap;
		}
	}
	*textWidth = s;

	return length;
}

struct ScrollPose {
	float s, y, z, ax, ay;
};

// Position and orient one letter on the wrapped strip, from its base place
// letterS to the pose a caller rotates and translates its model with.
// Returns false, pose untouched, once the letter is past cull on either
// side.
//
// raw = letterS - phase is the letter's place on an unwrapped line; it is
// brought to the representative closest to 0 every frame rather than
// carrying a wrap decision forward from the last one. phase itself resets
// by -textWidth once a lap (it is a fmod), which jumps raw by +textWidth for
// every letter at once, not just the one due to cross -cull; recomputing
// fresh from letterS and the current phase keeps a letter already inside the
// visible window from being caught by that reset and culled a lap early.
static bool ScrollPoseCompute(float letterS, double phase, float textWidth, float cull, float wavek,
							   float spin, float distance, float ampy, float ampz, ScrollPose *pose)
{
	float raw = letterS - (float)phase;
	if (raw > textWidth * 0.5f) {
		raw -= textWidth;
	} else if (raw < -textWidth * 0.5f) {
		raw += textWidth;
	}
	float s = raw + cull;
	if (s < -cull || s > cull) {
		return false;
	}

	float wave = s * wavek + spin;
	float dyds = ampy * wavek * cos(wave);
	float dzds = -ampz * wavek * sin(wave);

	pose->s = s;
	pose->y = ampy * sin(wave);
	pose->z = distance + ampz * cos(wave);
	pose->ax = atan2(dyds, 1.0f);
	pose->ay = atan2(-dzds, 1.0f);
	return true;
}

static int AddVertex(Model3D *model, float x, float y, float z)
{
	return RETRO_AddModelVertex(model, x, y, z);
}

// Every face of a glyph carries the same offset, added before the Lambert
// term (see the file header): this is not RETRO_AddModelQuad's neutral
// default, so the local wrapper is what every Add* helper below calls.
static void AddQuad(Model3D *model, int a, int b, int c, int d)
{
	RETRO_AddModelQuad(model, a, b, c, d, GLENZ_FACE);
}

static bool InkAt(const bool ink[MAX_GLYPH_SIZE][MAX_GLYPH_SIZE], int width, int height, int x, int y)
{
	return x >= 0 && y >= 0 && x < width && y < height && ink[y][x];
}

// Pixel rectangle [px0, px1) × [py0, py1) in the atlas cell, in model units.
// py = 0 is the top of the glyph, which is negative y.
static void PixelBounds(int px0, int py0, int px1, int py1, float halfw, float halfh, float *x0, float *y0, float *x1, float *y1)
{
	*x0 = (px0 - halfw) * Pixel;
	*x1 = (px1 - halfw) * Pixel;
	*y0 = (py0 - halfh) * Pixel;
	*y1 = (py1 - halfh) * Pixel;
}

static void AddFrontBack(Model3D *model, float x0, float y0, float x1, float y1, float z0, float z1)
{
	int v0 = AddVertex(model, x0, y1, z0); // front, down, left
	int v1 = AddVertex(model, x1, y1, z0); // front, down, right
	int v2 = AddVertex(model, x1, y0, z0); // front, up, right
	int v3 = AddVertex(model, x0, y0, z0); // front, up, left
	int v4 = AddVertex(model, x0, y0, z1); // back, up, left
	int v5 = AddVertex(model, x1, y0, z1); // back, up, right
	int v6 = AddVertex(model, x1, y1, z1); // back, down, right
	int v7 = AddVertex(model, x0, y1, z1); // back, down, left
	AddQuad(model, v0, v1, v2, v3); // front, −z
	AddQuad(model, v4, v5, v6, v7); // back, +z
}

static void AddTop(Model3D *model, float x0, float y, float x1, float z0, float z1)
{
	int a = AddVertex(model, x0, y, z0);
	int b = AddVertex(model, x1, y, z0);
	int c = AddVertex(model, x1, y, z1);
	int d = AddVertex(model, x0, y, z1);
	AddQuad(model, a, b, c, d); // up, −y
}

static void AddBottom(Model3D *model, float x0, float y, float x1, float z0, float z1)
{
	int a = AddVertex(model, x0, y, z1);
	int b = AddVertex(model, x1, y, z1);
	int c = AddVertex(model, x1, y, z0);
	int d = AddVertex(model, x0, y, z0);
	AddQuad(model, a, b, c, d); // down, +y
}

static void AddLeft(Model3D *model, float x, float y0, float y1, float z0, float z1)
{
	int a = AddVertex(model, x, y1, z0);
	int b = AddVertex(model, x, y0, z0);
	int c = AddVertex(model, x, y0, z1);
	int d = AddVertex(model, x, y1, z1);
	AddQuad(model, a, b, c, d); // left, −x
}

static void AddRight(Model3D *model, float x, float y0, float y1, float z0, float z1)
{
	int a = AddVertex(model, x, y1, z0);
	int b = AddVertex(model, x, y1, z1);
	int c = AddVertex(model, x, y0, z1);
	int d = AddVertex(model, x, y0, z0);
	AddQuad(model, a, b, c, d); // right, +x
}

//
// Extrude one atlas glyph into the working model
//
// Ink is packed into the fewest axis-aligned rectangles that cover it, then
// each rectangle is extruded along z. Adjacent pixels therefore share one
// front and one back rather than meeting as three-face cube corners, which
// under Glenz addition would wash those seams to white. Side walls are the
// exposed runs of that silhouette. Winding matches retrologo.obj: the front
// face (z = −depth/2) is listed down-left, down-right, up-right, up-left,
// which is the order RETRO_SortFaces reads as facing the eye at −z.
//
static void BuildGlyph(Model3D *model, unsigned char character)
{
	model->vertices = 0;
	model->faces = 0;
	model->normals = 0;

	int width = Font.width;
	int height = Font.height;
	if (width > MAX_GLYPH_SIZE || height > MAX_GLYPH_SIZE) {
		RETRO_RageQuit("Glyph is larger than the ink mask\n");
	}

	bool ink[MAX_GLYPH_SIZE][MAX_GLYPH_SIZE];
	memset(ink, 0, sizeof(ink));
	for (int py = 0; py < height; py++) {
		for (int px = 0; px < width; px++) {
			ink[py][px] = RETRO_FontInk(Font, character, px, py);
		}
	}

	float halfw = width * 0.5f;
	float halfh = height * 0.5f;
	float z0 = -LETTER_DEPTH * 0.5f;
	float z1 = LETTER_DEPTH * 0.5f;

	bool used[MAX_GLYPH_SIZE][MAX_GLYPH_SIZE];
	memset(used, 0, sizeof(used));

	for (int py = 0; py < height; py++) {
		for (int px = 0; px < width; px++) {
			if (!ink[py][px] || used[py][px]) {
				continue;
			}

			int rw = 1;
			while (px + rw < width && ink[py][px + rw] && !used[py][px + rw]) {
				rw++;
			}

			int rh = 1;
			while (py + rh < height) {
				bool row = true;
				for (int dx = 0; dx < rw; dx++) {
					if (!ink[py + rh][px + dx] || used[py + rh][px + dx]) {
						row = false;
						break;
					}
				}
				if (!row) {
					break;
				}
				rh++;
			}

			for (int dy = 0; dy < rh; dy++) {
				for (int dx = 0; dx < rw; dx++) {
					used[py + dy][px + dx] = true;
				}
			}

			float x0, y0, x1, y1;
			PixelBounds(px, py, px + rw, py + rh, halfw, halfh, &x0, &y0, &x1, &y1);
			AddFrontBack(model, x0, y0, x1, y1, z0, z1);
		}
	}

	for (int py = 0; py < height; py++) {
		int px = 0;
		while (px < width) {
			if (!(ink[py][px] && !InkAt(ink, width, height, px, py - 1))) {
				px++;
				continue;
			}
			int px0 = px;
			while (px < width && ink[py][px] && !InkAt(ink, width, height, px, py - 1)) {
				px++;
			}
			float x0, y0, x1, y1;
			PixelBounds(px0, py, px, py + 1, halfw, halfh, &x0, &y0, &x1, &y1);
			AddTop(model, x0, y0, x1, z0, z1);
		}

		px = 0;
		while (px < width) {
			if (!(ink[py][px] && !InkAt(ink, width, height, px, py + 1))) {
				px++;
				continue;
			}
			int px0 = px;
			while (px < width && ink[py][px] && !InkAt(ink, width, height, px, py + 1)) {
				px++;
			}
			float x0, y0, x1, y1;
			PixelBounds(px0, py, px, py + 1, halfw, halfh, &x0, &y0, &x1, &y1);
			AddBottom(model, x0, y1, x1, z0, z1);
		}
	}

	for (int px = 0; px < width; px++) {
		int py = 0;
		while (py < height) {
			if (!(ink[py][px] && !InkAt(ink, width, height, px - 1, py))) {
				py++;
				continue;
			}
			int py0 = py;
			while (py < height && ink[py][px] && !InkAt(ink, width, height, px - 1, py)) {
				py++;
			}
			float x0, y0, x1, y1;
			PixelBounds(px, py0, px + 1, py, halfw, halfh, &x0, &y0, &x1, &y1);
			AddLeft(model, x0, y0, y1, z0, z1);
		}

		py = 0;
		while (py < height) {
			if (!(ink[py][px] && !InkAt(ink, width, height, px + 1, py))) {
				py++;
				continue;
			}
			int py0 = py;
			while (py < height && ink[py][px] && !InkAt(ink, width, height, px + 1, py)) {
				py++;
			}
			float x0, y0, x1, y1;
			PixelBounds(px, py0, px + 1, py, halfw, halfh, &x0, &y0, &x1, &y1);
			AddRight(model, x1, y0, y1, z0, z1);
		}
	}

	if (model->faces > 0) {
		RETRO_InitializeFaceNormals(model);
	}
}

// One built mesh per distinct character, since a glyph's ink pattern never
// changes - only its rotate/translate/project pose does, every frame, in
// DEMO_Render. Built lazily so only the characters ScrollText actually uses
// ever cost a BuildGlyph call.
static Model3D *GetGlyph(unsigned char character)
{
	for (int i = 0; i < GlyphCacheCount; i++) {
		if (GlyphCacheChar[i] == character) {
			return GlyphCache[i];
		}
	}
	if (GlyphCacheCount >= MAX_DISTINCT_GLYPHS) {
		RETRO_RageQuit("Too many distinct glyphs in the scroll text\n");
	}

	Model3D *model = RETRO_Allocate3DModel();
	model->c = 0;
	model->shades = GLENZ_SHADES;
	model->colormax = 3 * GLENZ_SHADES; // hold the add at the gradient's last entry, or a third overlap would walk into white
	BuildGlyph(model, character);

	GlyphCache[GlyphCacheCount] = model;
	GlyphCacheChar[GlyphCacheCount] = character;
	GlyphCacheCount++;
	return model;
}

void DEMO_Render(double time, double deltatime)
{
	double phase = fmod(time * SCROLL_SPEED, TextWidth);
	if (phase < 0) {
		phase += TextWidth;
	}
	float spin = (float)(time * HELIX_SPIN);

	for (int i = 0; i < TextLength; i++) {
		unsigned char character = (unsigned char)ScrollText[i];
		if (character == ' ') {
			continue;
		}

		// ax tips the top toward the camera when the path drops; ay sends
		// the right side away when the path recedes; the spin and rock are
		// the cube's turn, so N · L actually moves.
		ScrollPose pose;
		if (!ScrollPoseCompute(LetterS[i], phase, TextWidth, CULL, HELIX_WAVE, spin,
								HELIX_DISTANCE, HELIX_AMP_Y, HELIX_AMP_Z, &pose)) {
			continue;
		}

		Model3D *glyph = GetGlyph(character);
		if (glyph->faces == 0) {
			continue;
		}

		float rock = fmod(time * LETTER_ROCK_SPEED, 2 * M_PI);
		float ax = pose.ax + LETTER_ROCK * sin(rock);
		float ay = pose.ay + fmod(time * LETTER_SPIN, 2 * M_PI);
		float az = LETTER_ROCK * cos(rock);

		RETRO_RotateModel(ax, ay, az, glyph);
		RETRO_TranslateModel(pose.s, pose.y, pose.z, glyph);
		RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, glyph);
		// Glenz adds into the framebuffer and does not use the q-buffer, so
		// overlapping letters and the front and back of one letter all show
		// through each other.
		RETRO_RenderModel(RETRO_POLY_GLENZ, RETRO_SHADE_FLAT, glyph, false);
	}
}

void DEMO_Initialize(void)
{
	// Single faces shade from black into magenta; past 3 · shades the ramp
	// keeps going into white, so a genuine triple overlap reads as a bright
	// highlight rather than flattening out.
	RETRO_CreateGradientPalette(8, 3 * GLENZ_SHADES, RETRO_BLACK, RETRO_MAGENTA);
	RETRO_CreateGradientPalette(3 * GLENZ_SHADES, RETRO_COLORS, RETRO_MAGENTA, RETRO_WHITE);

	Font = RETRO_LoadFont(FONT);
	Pixel = LETTER_HEIGHT / Font.height;

	TextLength = BuildLetterStrip(ScrollText, Font, Pixel, LETTER_GAP, SPACE_WIDTH,
								   LetterS, MAX_SCROLL_LETTERS, &TextWidth);

	RETRO_InitializeLightSource(0, 0, -1);
}
