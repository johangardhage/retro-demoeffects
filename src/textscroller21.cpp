//
// A ring of letters spinning about a tilted axis. Each letter is its own
// textured quad. The letters lie on the faces of a convex prism, so those
// facing the eye never overlap each other, nor do those facing away, and
// every facing letter is in front of every averted one. Each half is then
// one flat layer, and its ink is the mask it is composited through: the far
// half over the background, the near half over that.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropoly.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
static const char *const RingText[] = { "RETRO DEMOEFFECTS * " };
#define LETTER_WIDTH 26.0f
#define LETTER_HEIGHT 40.0f
#define LETTER_GAP 2.0f
#define SPIN_SPEED 0.6
#define TILT 0.1f
#define WOBBLE 0.22f
#define WOBBLE_SPEED 0.7
#define CAMERA_DISTANCE 300.0f

// Letters facing away are drawn darker, from their own copy of the strip.
#define SHADES 8
#define INK 1
#define SKY 16
#define SKY_SHADES 64

static RETRO_Font Font;
static int LetterCount;
static float Radius;
static RETRO_Image *TextStrip[SHADES];
static unsigned char Background[RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char Scene[RETRO_WIDTH * RETRO_HEIGHT];

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
// cleared frame. A letter faces the eye when its quad keeps its winding on
// screen.
static void DrawLetters(float ax, float ay, bool facing)
{
	memset(RETRO_FrameBuffer(), 0, RETRO_WIDTH * RETRO_HEIGHT);
	RETRO_ClearDepthBuffer();
	float step = (float)(2 * M_PI / LetterCount);

	for (int i = 0; i < LetterCount; i++) {
		mat3 matrix = rotateX(ax) * rotateY(ay - i * step);
		float u = (float)(i * Font.width);
		PolygonPoint quad[4] = {
			Project(matrix, -LETTER_WIDTH / 2, -LETTER_HEIGHT / 2, { u, 0 }),
			Project(matrix, LETTER_WIDTH / 2, -LETTER_HEIGHT / 2, { u + Font.width, 0 }),
			Project(matrix, LETTER_WIDTH / 2, LETTER_HEIGHT / 2, { u + Font.width, (float)Font.height }),
			Project(matrix, -LETTER_WIDTH / 2, LETTER_HEIGHT / 2, { u, (float)Font.height })
		};
		if ((cross(quad[1].pos - quad[0].pos, quad[3].pos - quad[0].pos) > 0) != facing) continue;

		// 1 facing the eye, 0 facing straight away.
		vec3 normal = matrix * vec3{ 0, 0, -1 };
		int shade = (int)((1.0f - normal.z) / 2.0f * (SHADES - 1) + 0.5f);
		RETRO_Image *strip = TextStrip[shade];
		RETRO_DrawTexMapPolygon(quad, 4, strip->data, strip->width, strip->height);
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *screen = RETRO_FrameBuffer();
	float ay = (float)fmod(time * SPIN_SPEED, 2 * M_PI);
	float ax = TILT + WOBBLE * (float)sin(fmod(time * WOBBLE_SPEED, 2 * M_PI));

	DrawLetters(ax, ay, false);
	for (int pixel = 0; pixel < RETRO_WIDTH * RETRO_HEIGHT; pixel++)
		Scene[pixel] = screen[pixel] ? screen[pixel] : Background[pixel];

	DrawLetters(ax, ay, true);
	for (int pixel = 0; pixel < RETRO_WIDTH * RETRO_HEIGHT; pixel++)
		if (!screen[pixel]) screen[pixel] = Scene[pixel];
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_Palette{ 0, 0, 0 });
	for (int i = 0; i < SHADES; i++) {
		float light = 0.25f + 0.75f * i / (SHADES - 1);
		RETRO_SetColor(INK + i, RETRO_Palette{ (unsigned char)(255 * light),
			(unsigned char)(200 * light), (unsigned char)(90 * light) });
	}
	for (int i = 0; i < SKY_SHADES; i++) {
		float light = sinf((float)M_PI * i / (SKY_SHADES - 1));
		RETRO_SetColor(SKY + i, RETRO_Palette{ (unsigned char)(20 + 30 * light),
			(unsigned char)(10 + 40 * light), (unsigned char)(50 + 80 * light) });
	}

	Font = RETRO_LoadFont(FONT);
	LetterCount = (int)strlen(RingText[0]);
	Radius = LetterCount * (LETTER_WIDTH + LETTER_GAP) / (float)(2 * M_PI);
	for (int shade = 0; shade < SHADES; shade++) {
		TextStrip[shade] = RETRO_GenerateTextImage(Font, RingText, 1);
		RETRO_Image *strip = TextStrip[shade];
		for (int i = 0; i < strip->width * strip->height; i++)
			strip->data[i] = strip->data[i] != 0 ? INK + shade : 0;
	}

	for (int y = 0; y < RETRO_HEIGHT; y++)
		memset(Background + y * RETRO_WIDTH, SKY + y * SKY_SHADES / RETRO_HEIGHT, RETRO_WIDTH);
}
