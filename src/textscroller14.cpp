//
// Letters enter from the left behind a blue panel, turn around on the
// right, then travel straight off the left edge in front.
// A generated text strip supplies perspective-correct glyph quads. Binary
// masks preserve the holes in letters; a dithered highlight lights the front.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropoly.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
static const char *const ScrollText[] = { "RETRO DEMOEFFECTS..." };
#define RADIUS 128.0f
#define LETTER_WIDTH 48.0f
#define LETTER_HEIGHT 108.0f
#define TURN_SPEED 2.5 // radians per second along the path
#define LETTER_DELAY 0.25 // seconds between letters, independent of message length
#define GAP_LETTERS 4 // empty letter slots before the message repeats
#define PERIOD ((LetterCount + GAP_LETTERS) * LETTER_DELAY)
#define CAMERA_DISTANCE 300.0f
#define TOP 24
#define BOTTOM 216

// A letter starts on the back straight with its right edge just off the left
// edge, and is done once its right edge has left on the front straight.
#define ENTRY_ANGLE 5.2
#define EXIT_OFFSET (-RETRO_WIDTH / 2.0f * (CAMERA_DISTANCE - RADIUS) / CAMERA_DISTANCE - LETTER_WIDTH / 2)

// A point light just left of and below the eye. Its Blinn-Phong highlight
// sits left of center on the front straight and slides over each letter.
#define EYE vec3{ 0, 0, -CAMERA_DISTANCE }
#define LIGHT vec3{ -29, 9, -CAMERA_DISTANCE }
#define SHININESS 80.0f

static RETRO_Font Font;
static int LetterCount;
static RETRO_Image *TextStrip;
static unsigned char Background[RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char Scene[RETRO_WIDTH * RETRO_HEIGHT];
static float SceneDepth[RETRO_WIDTH * RETRO_HEIGHT];
static const int Dither[4][4] = {
	{ 0, 8, 2, 10 }, { 12, 4, 14, 6 },
	{ 3, 11, 1, 9 }, { 15, 7, 13, 5 }
};

static PolygonPoint Project(const mat3 &matrix, float offset, float x, float y, vec2 uv)
{
	Vertex vertex = {};
	vertex.pos = { x, y, -RADIUS };
	RETRO_RotateVertex(&vertex, matrix);
	vertex.rpos.x += offset;
	RETRO_ProjectVertex(&vertex, 1.0f, RETRO_WIDTH / 2.0f,
		RETRO_HEIGHT / 2.0f, CAMERA_DISTANCE);
	return { vertex.spos, 0, uv, vertex.q };
}

void DEMO_Render(double time, double deltatime)
{
	memcpy(Scene, Background, sizeof(Scene));
	memset(SceneDepth, 0, sizeof(SceneDepth));
	unsigned char *screen = RETRO_FrameBuffer();

	for (int i = 0; i < LetterCount; i++) {
		double age = time - i * LETTER_DELAY;
		if (age < 0) continue;
		double angle = ENTRY_ANGLE - fmod(age, PERIOD) * TURN_SPEED;
		// Straight tangents join the back and front of the right-hand turn.
		// Clamp the orientation on both straight sections so entry and exit
		// translate the letters without rotating them.
		float offset = 0.0f;
		if (angle > M_PI) offset = (float)((M_PI - angle) * RADIUS);
		if (angle < 0) offset = (float)(angle * RADIUS);
		if (angle < 0 && offset < EXIT_OFFSET) continue;
		double turn = angle > M_PI ? M_PI : MAX(angle, 0.0);
		mat3 matrix = rotateY((float)-turn);
		vec3 normal = matrix * vec3{ 0, 0, -1 };
		float u = (float)(i * Font.width);
		PolygonPoint quad[4] = {
			Project(matrix, offset, -LETTER_WIDTH / 2, -LETTER_HEIGHT / 2, { u, 0 }),
			Project(matrix, offset, LETTER_WIDTH / 2, -LETTER_HEIGHT / 2, { u + Font.width, 0 }),
			Project(matrix, offset, LETTER_WIDTH / 2, LETTER_HEIGHT / 2, { u + Font.width, (float)Font.height }),
			Project(matrix, offset, -LETTER_WIDTH / 2, LETTER_HEIGHT / 2, { u, (float)Font.height })
		};

		// The letter's screen bounds; nothing outside them is cleared, drawn or read.
		vec2 low = quad[0].pos, high = quad[0].pos;
		for (const PolygonPoint &point : quad) {
			low = { MIN(low.x, point.pos.x), MIN(low.y, point.pos.y) };
			high = { MAX(high.x, point.pos.x), MAX(high.y, point.pos.y) };
		}
		ClipRect box = { MAX((int)floor(low.x), 0), MIN((int)ceil(high.x) + 1, RETRO_WIDTH),
			MAX((int)floor(low.y), TOP), MIN((int)ceil(high.y) + 1, BOTTOM) };
		if (box.x0 >= box.x1 || box.y0 >= box.y1) continue;

		// Render one mask, then composite only its ink using real depth.
		// This lets the background and other letters show through holes.
		for (int y = box.y0; y < box.y1; y++) {
			memset(screen + y * RETRO_WIDTH + box.x0, 0, box.x1 - box.x0);
			memset(RETRO_DepthBuffer + y * RETRO_WIDTH + box.x0, 0, (box.x1 - box.x0) * sizeof(float));
		}
		RETRO_DrawTexMapPolygon(quad, 4, TextStrip->data,
			TextStrip->width, TextStrip->height, false, box);
		for (int y = box.y0; y < box.y1; y++)
			for (int x = box.x0; x < box.x1; x++) {
				int pixel = y * RETRO_WIDTH + x;
				float q = RETRO_DepthBuffer[pixel];
				if (!screen[pixel] || q <= SceneDepth[pixel]) continue;
				SceneDepth[pixel] = q;
				bool behind = q < 1.0f / CAMERA_DISTANCE;
				unsigned char color = 3;
				if (behind && Background[pixel]) {
					color = Background[pixel] == 1 ? 4 : 5;
				} else if (!behind) {
					// The pixel's point on the letter, back from its depth, lit by
					// the light's highlight on whichever face is toward the eye.
					float depth = 1.0f / q;
					vec3 point = { (x + 0.5f - RETRO_WIDTH / 2.0f) * depth / CAMERA_DISTANCE,
						(y + 0.5f - RETRO_HEIGHT / 2.0f) * depth / CAMERA_DISTANCE, depth - CAMERA_DISTANCE };
					vec3 view = normalize(EYE - point);
					vec3 facing = dot(normal, view) < 0 ? -normal : normal;
					vec3 halfway = normalize(normalize(LIGHT - point) + view);
					float shade = 3.0f * pow(MAX(dot(facing, halfway), 0.0f), SHININESS);
					int level = (int)shade;
					if (shade - level > (Dither[y & 3][x & 3] + 0.5f) / 16.0f) level++;
					color = (unsigned char)(6 + level);
				}
				Scene[pixel] = color;
			}
	}
	memcpy(screen, Scene, sizeof(Scene));
}

void DEMO_Initialize(void)
{
	RETRO_Palette colors[] = {
		{ 0, 0, 0 }, { 68, 82, 118 }, { 33, 49, 84 },
		{ 85, 132, 81 }, { 66, 102, 116 }, { 40, 78, 89 },
		{ 125, 202, 55 }, { 166, 224, 99 }, { 208, 242, 150 }, { 239, 255, 196 }
	};
	for (int i = 0; i < (int)(sizeof(colors) / sizeof(colors[0])); i++)
		RETRO_SetColor(i, colors[i]);

	Font = RETRO_LoadFont(FONT);
	LetterCount = (int)strlen(ScrollText[0]);
	TextStrip = RETRO_GenerateTextImage(Font, ScrollText, 1);
	for (int i = 0; i < TextStrip->width * TextStrip->height; i++)
		TextStrip->data[i] = TextStrip->data[i] != 0;

	// Fixed zigzag silhouette, split into two blue bands.
	const vec2 edge[] = { { 160, TOP }, { 188, 51 }, { 174, 66 },
		{ 202, 91 }, { 160, 133 }, { 188, 161 }, { 174, 176 },
		{ 188, 190 }, { 160, BOTTOM } };
	for (int segment = 0; segment < 8; segment++) {
		vec2 a = edge[segment], b = edge[segment + 1];
		for (int y = (int)a.y; y < (int)b.y; y++) {
			int right = (int)(a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y));
			for (int x = 0; x < right; x++)
				Background[y * RETRO_WIDTH + x] = y < 133 ? 1 : 2;
		}
	}
}
