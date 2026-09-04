//
// 3D polygon scroller
//
// Each glyph of the font (see FONT below) is extruded into a solid of cubes,
// one cube per ink pixel, so every letter is a mesh of quads rather than a
// blit. Faces two cubes share are omitted: a wall buried in the letter is
// inside the solid, and a wall drawn at a grazing angle spills a pixel past
// its own edge, which on a buried wall lands on the lit face in front of it.
// The remaining faces are convex quads, which is what the polygon drawer can
// fill. Model y is the screen's, growing down, and the front of a letter
// faces the eye at −z.
//
// Letter i travels the helix
//
//   s = s_i − phase + CULL
//   y = A_y sin(k s + ω t)
//   z = D + A_z cos(k s + ω t)
//
// wrapped onto one period of the strip so a letter that leaves on the left
// re-enters on the right. s_i is the letter's place along the strip. The
// letter then turns to the tangent (∂y/∂s, ∂z/∂s), so it banks with the
// helix rather than sliding through it upright. Flat Lambert, one term per
// face: a cube face has one normal, and a specular highlight would flash the
// whole of it. Depth is a shared q-buffer, because letters interleave as
// they pass. Euler angles live on 2π.
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

#define MAX_SCROLL_LETTERS 256

static const char ScrollText[] = "RETRO DEMOEFFECTS...           ";

#define MAX_DISTINCT_GLYPHS 32 // more than the alphabet any one ScrollText is likely to use

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

//
// Extrude one atlas glyph into the working model
//
// Pixel (px, py) of an 8-wide cell occupies x ∈ [px − w/2, px + 1 − w/2] · Pixel
// and y ∈ [py − h/2, py + 1 − h/2] · Pixel, so the glyph is centred on the
// origin and py = 0 is the top, which is negative y. The cube is extruded to
// z ∈ [−depth/2, +depth/2]. Winding matches retrologo.obj: the front face
// (z = −depth/2) is listed down-left, down-right, up-right, up-left, which is
// the order RETRO_SortFaces reads as facing the eye at −z.
//
static void BuildGlyph(Model3D *model, unsigned char character)
{
	model->vertices = 0;
	model->faces = 0;
	model->normals = 0;

	float halfw = Font.width * 0.5f;
	float halfh = Font.height * 0.5f;
	float z0 = -LETTER_DEPTH * 0.5f;
	float z1 = LETTER_DEPTH * 0.5f;

	for (int py = 0; py < Font.height; py++) {
		for (int px = 0; px < Font.width; px++) {
			if (!RETRO_FontInk(Font, character, px, py)) {
				continue;
			}

			float x0 = (px - halfw) * Pixel;
			float x1 = (px + 1 - halfw) * Pixel;
			float y0 = (py - halfh) * Pixel;
			float y1 = (py + 1 - halfh) * Pixel;

			int v0 = RETRO_AddModelVertex(model, x0, y1, z0); // front, down, left
			int v1 = RETRO_AddModelVertex(model, x1, y1, z0); // front, down, right
			int v2 = RETRO_AddModelVertex(model, x1, y0, z0); // front, up, right
			int v3 = RETRO_AddModelVertex(model, x0, y0, z0); // front, up, left
			int v4 = RETRO_AddModelVertex(model, x0, y0, z1); // back, up, left
			int v5 = RETRO_AddModelVertex(model, x1, y0, z1); // back, up, right
			int v6 = RETRO_AddModelVertex(model, x1, y1, z1); // back, down, right
			int v7 = RETRO_AddModelVertex(model, x0, y1, z1); // back, down, left

			RETRO_AddModelQuad(model, v0, v1, v2, v3); // front, −z
			RETRO_AddModelQuad(model, v4, v5, v6, v7); // back, +z
			if (!RETRO_FontInk(Font, character, px, py + 1)) {
				RETRO_AddModelQuad(model, v7, v6, v1, v0); // down, +y
			}
			if (!RETRO_FontInk(Font, character, px, py - 1)) {
				RETRO_AddModelQuad(model, v3, v2, v5, v4); // up, −y
			}
			if (!RETRO_FontInk(Font, character, px - 1, py)) {
				RETRO_AddModelQuad(model, v0, v3, v4, v7); // left, −x
			}
			if (!RETRO_FontInk(Font, character, px + 1, py)) {
				RETRO_AddModelQuad(model, v1, v6, v5, v2); // right, +x
			}
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
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;
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

	// All letters share one depth buffer. Clearing before each glyph would
	// make a later letter overwrite an earlier one regardless of its depth.
	RETRO_ClearDepthBuffer();

	for (int i = 0; i < TextLength; i++) {
		unsigned char character = (unsigned char)ScrollText[i];
		if (character == ' ') {
			continue;
		}

		// ax tips the top toward the camera when the path drops (y grows
		// down); ay sends the right side away when the path recedes.
		ScrollPose pose;
		if (!ScrollPoseCompute(LetterS[i], phase, TextWidth, CULL, HELIX_WAVE, spin,
								HELIX_DISTANCE, HELIX_AMP_Y, HELIX_AMP_Z, &pose)) {
			continue;
		}

		Model3D *glyph = GetGlyph(character);
		if (glyph->faces == 0) {
			continue;
		}

		RETRO_RotateModel(pose.ax, pose.ay, 0, glyph);
		RETRO_TranslateModel(pose.s, pose.y, pose.z, glyph);
		RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, glyph);
		RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT, glyph, false);
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Matte, because a flat lit face has one normal for all of
	// it and a specular highlight would flash the whole face at once
	RETRO_CreateMattePalette(RETRO_PINK);

	Font = RETRO_LoadFont(FONT);
	Pixel = LETTER_HEIGHT / Font.height;

	TextLength = BuildLetterStrip(ScrollText, Font, Pixel, LETTER_GAP, SPACE_WIDTH,
								   LetterS, MAX_SCROLL_LETTERS, &TextWidth);

	RETRO_InitializeLightSource(0, 0, -1);
}
