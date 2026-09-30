//
// Scroller, through a ring of letters
//
// A ring of letters spinning about a tilted axis, with a message longer than
// the ring scrolling through it. The ring has room for RING_LETTERS letters.
// The message is wound on it like a thread on a reel, and the ring shows the
// one turn of it that is nearest the front: letter n stands at the angle
//
//   (phase - n) 2π / RING_LETTERS
//
// from the front, phase being how many letters have gone past it, and the
// letters drawn are the RING_LETTERS of them within half a turn. So a letter
// comes in at the middle of the back, takes the place of the one that leaves
// there, goes round one side to the front and on round the other, and is
// gone when it gets to the back again. The message has one empty turn of the
// ring before it, so it starts with its first letter coming in at the back of
// an empty ring, and the ring empties again before it starts over. phase
// lives on the length of the two together.
//
// A letter does not come in and leave all at once. It is nothing at the
// middle of the back and solid FADE_LETTERS places from there, on either
// side, and in between it is as solid as it is far along: of every sixteen
// pixels of its ink, laid out in a 4×4 ordered dither over the screen, as
// many are drawn as it has of FADE_LEVELS sixteenths, and the background
// shows through the rest.
//
// Each letter is its own textured quad. The letters lie on the faces of a
// convex prism, so those facing the eye never overlap each other, nor do
// those facing away, and every facing letter is in front of every averted
// one. Each half is then one flat layer, and its ink is the mask it is
// composited through: the far half over the background, the near half over
// that. The far half, where the fading is, is drawn twice, once in the shades
// of its letters and once in how solid they are, and the second is the mask
// for the first.
//
// A letter is flat, so it has one shade. A lamp up and to the left of the eye
// lights it by the cosine of the angle between LIGHT and the side of the
// letter that is seen, the outside of one facing the eye and the inside of
// one facing away, over a floor of AMBIENT. That takes it up the first
// GLEAM_START of a ramp of SHADES entries, from a dark gold to the full one.
// The rest of the ramp goes on to a pale gleam, and a letter gets up it by
// the cosine to the power of SHININESS, so only one turned almost straight at
// the lamp does: the letters differ round the ring, and each flares up as it
// passes the lamp. The far side of the ring is dimmed down to BACK_LIGHT of
// all this, so it stays behind the near side to the eye as well.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropalette.h"
#include "lib/retropoly.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
static const char *const Message[] = { "RETRO DEMOEFFECTS..." };
#define RING_LETTERS 30 // letters the ring has room for
#define LETTER_WIDTH 26.0f
#define LETTER_HEIGHT 40.0f
#define LETTER_GAP 2.0f
#define SCROLL_SPEED 2.5 // letters per second
#define FADE_LETTERS 4.0f // places from the middle of the back to where a letter is solid
#define FADE_LEVELS 16 // steps from nothing to solid
#define TILT 0.1f
#define WOBBLE 0.22f
#define WOBBLE_SPEED 0.7
#define CAMERA_DISTANCE 400.0f
#define LIGHT vec3{ -0.45f, -0.35f, -0.82f } // toward the lamp: left, up and out of the screen
#define AMBIENT 0.35f // light on a letter turned edge on to the lamp
#define BACK_LIGHT 0.4f // what is left of the light at the back of the ring
#define SHININESS 12 // how sharply the gleam falls off as a letter turns from the lamp
#define GLEAM_START 0.7f // part of the ramp below the gleam

// A letter is drawn in its shade from that shade's own copy of the strip.
#define SHADES 32
#define INK 1
#define SKY (INK + SHADES)
#define SKY_SHADES 64

static RETRO_Font Font;
static int MessageLength;
static float Radius;
static RETRO_Image *Strip;
static unsigned char *TextStrip[SHADES];
static unsigned char *FadeStrip[FADE_LEVELS];
static unsigned char Background[RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char Scene[RETRO_WIDTH * RETRO_HEIGHT];

// 4×4 ordered dither thresholds, in sixteenths
static const unsigned char Bayer[4][4] = {
	{ 0, 8, 2, 10 },
	{ 12, 4, 14, 6 },
	{ 3, 11, 1, 9 },
	{ 15, 7, 13, 5 },
};

static PolygonPoint Project(const mat3 &matrix, float x, float y, vec2 uv)
{
	Vertex vertex = {};
	vertex.pos = { x, y, -Radius };
	RETRO_RotateVertex(&vertex, matrix);
	RETRO_ProjectVertex(&vertex, 1.0f, RETRO_WIDTH / 2.0f,
		RETRO_HEIGHT / 2.0f, CAMERA_DISTANCE);
	return { vertex.spos, 0, uv, vertex.q };
}

// Draw the letters that face the eye, or those that face away, into a
// cleared frame, in their shades or else in how solid they are, from 1 to
// FADE_LEVELS. A letter faces the eye when its quad keeps its winding on
// screen.
static void DrawLetters(float ax, float phase, bool facing, bool solidity)
{
	memset(RETRO_FrameBuffer(), 0, RETRO_WIDTH * RETRO_HEIGHT);
	RETRO_ClearDepthBuffer();
	float step = (float)(2 * M_PI / RING_LETTERS);
	int first = (int)ceilf(phase - RING_LETTERS / 2.0f);

	for (int i = first; i < first + RING_LETTERS; i++) {
		mat3 matrix = rotateX(ax) * rotateY((phase - i) * step);
		int letter = WRAP(i, RING_LETTERS + MessageLength) - RING_LETTERS;
		if (letter < 0) continue;
		float u = (float)(letter * Font.width);
		PolygonPoint quad[4] = {
			Project(matrix, -LETTER_WIDTH / 2, -LETTER_HEIGHT / 2, { u, 0 }),
			Project(matrix, LETTER_WIDTH / 2, -LETTER_HEIGHT / 2, { u + Font.width, 0 }),
			Project(matrix, LETTER_WIDTH / 2, LETTER_HEIGHT / 2, { u + Font.width, (float)Font.height }),
			Project(matrix, -LETTER_WIDTH / 2, LETTER_HEIGHT / 2, { u, (float)Font.height })
		};
		if ((cross(quad[1].pos - quad[0].pos, quad[3].pos - quad[0].pos) > 0) != facing) continue;

		if (solidity) {
			// Nothing at the middle of the back, solid FADE_LETTERS places from it.
			float solid = CLAMP01((RING_LETTERS / 2.0f - fabsf(phase - i)) / FADE_LETTERS);
			int level = (int)(solid * FADE_LEVELS);
			if (level > 0) RETRO_DrawTexMapPolygon(quad, 4, FadeStrip[level - 1], Strip->width, Strip->height);
			continue;
		}

		// The side that is seen is lit, less the further back on the ring it is.
		vec3 normal = matrix * vec3{ 0, 0, -1 };
		float lambert = MAX(dot(facing ? normal : -normal, normalize(LIGHT)), 0.0f);
		float light = GLEAM_START * (AMBIENT + (1 - AMBIENT) * lambert) + (1 - GLEAM_START) * powf(lambert, SHININESS);
		float dim = 1 - (1 - BACK_LIGHT) * (normal.z + 1) / 2;
		int shade = CLAMP((int)(light * dim * SHADES), 0, SHADES);
		RETRO_DrawTexMapPolygon(quad, 4, TextStrip[shade], Strip->width, Strip->height);
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *screen = RETRO_FrameBuffer();
	float phase = (float)fmod(time * SCROLL_SPEED + RING_LETTERS / 2.0, RING_LETTERS + MessageLength);
	float ax = TILT + WOBBLE * (float)sin(fmod(time * WOBBLE_SPEED, 2 * M_PI));

	DrawLetters(ax, phase, false, false);
	memcpy(Scene, screen, RETRO_WIDTH * RETRO_HEIGHT);

	// A pixel of the far half keeps its ink where its letter is solid enough
	// for the dither there. Where there is no letter it is not solid at all.
	DrawLetters(ax, phase, false, true);
	for (int y = 0; y < RETRO_HEIGHT; y++)
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int pixel = y * RETRO_WIDTH + x;
			if (screen[pixel] <= Bayer[y & 3][x & 3]) Scene[pixel] = Background[pixel];
		}

	DrawLetters(ax, phase, true, false);
	for (int pixel = 0; pixel < RETRO_WIDTH * RETRO_HEIGHT; pixel++)
		if (!screen[pixel]) screen[pixel] = Scene[pixel];
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_Palette{ 0, 0, 0 });
	int gleam = INK + (int)(GLEAM_START * SHADES);
	RETRO_CreateGradientPalette(INK, gleam, RETRO_Palette{ 40, 28, 8 }, RETRO_Palette{ 255, 200, 90 });
	RETRO_CreateGradientPalette(gleam, INK + SHADES, RETRO_Palette{ 255, 200, 90 }, RETRO_Palette{ 255, 250, 215 });
	for (int i = 0; i < SKY_SHADES; i++) {
		float light = sinf((float)M_PI * i / (SKY_SHADES - 1));
		RETRO_SetColor(SKY + i, RETRO_Palette{ (unsigned char)(20 + 30 * light),
			(unsigned char)(10 + 40 * light), (unsigned char)(50 + 80 * light) });
	}

	Font = RETRO_LoadFont(FONT);
	MessageLength = (int)strlen(Message[0]);
	Radius = RING_LETTERS * (LETTER_WIDTH + LETTER_GAP) / (float)(2 * M_PI);
	Strip = RETRO_GenerateTextImage(Font, Message, 1);
	for (int shade = 0; shade < SHADES; shade++) {
		TextStrip[shade] = (unsigned char *)malloc(Strip->width * Strip->height);
		for (int i = 0; i < Strip->width * Strip->height; i++)
			TextStrip[shade][i] = Strip->data[i] != 0 ? INK + shade : 0;
	}
	for (int level = 0; level < FADE_LEVELS; level++) {
		FadeStrip[level] = (unsigned char *)malloc(Strip->width * Strip->height);
		for (int i = 0; i < Strip->width * Strip->height; i++)
			FadeStrip[level][i] = Strip->data[i] != 0 ? level + 1 : 0;
	}

	for (int y = 0; y < RETRO_HEIGHT; y++)
		memset(Background + y * RETRO_WIDTH, SKY + y * SKY_SHADES / RETRO_HEIGHT, RETRO_WIDTH);
}
