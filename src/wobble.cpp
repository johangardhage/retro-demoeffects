//
// Wobble
//
// A picture seen through a rippled circle. The screen's border is laid onto
// the circle, a quarter turn to each edge, so the corners sit at four points a
// quarter turn apart and each edge follows the arc between its two corners.
// The ripple is a sine on the circle's radius, a whole number of waves around
// it, rolling as the circle turns. Every pixel inside then reads the picture
// where the four edges put it: the top and bottom edges are blended down each
// column, and the left and right edges' bulge away from the straight lines
// between their corners is added across each row.
//
// The picture is 256x256 and wraps both ways. Five stages run in turn, each
// fading in from black and back out, the second and the last with the
// ripple's depth breathing.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retrovector.h"
#include "lib/retropalette.h"
#include "lib/retromain.h"

constexpr char PICTURE[] = "assets/einstein_256x256.pcx";
constexpr vec2 PICTURE_OFFSET = { 215, 0 }; // texels the picture is moved by

constexpr float START_ANGLE = radians(225); // the angle of the top left corner as a stage starts
constexpr float QUARTER_TURN = radians(90);
constexpr float SWING_SPEED = radians(615); // how fast the ripple's depth swings, a second
constexpr float FADE_TIME = 0.45f;         // seconds to fade in, and out

struct Stage {
	float duration;                        // seconds the stage lasts
	float radius;                          // the circle's radius, in texels
	float spin;                            // degrees a second the circle turns
	float roll;                            // degrees a second the ripple rolls round it
	float waves;                           // the ripple's waves round the circle
	float depth;                           // the ripple's depth, in texels
	float swing;                           // texels the depth swings either way, if it does
};

static const Stage Stages[] = {
	{ 3.7f, -36, -68, 98, 16, 6, 0 },
	{ 5.6f, 84, 197, -910, 4, 32, 32 },
	{ 5.6f, 412, 6, 74, 4, -161, 0 },
	{ 5.6f, 108, -135, 787, 8, -44, 0 },
	{ 5.6f, 84, 0, 98, 1, 160, 96 },
};
constexpr int STAGE_COUNT = sizeof(Stages) / sizeof(Stages[0]);

static vec2 Top[RETRO_WIDTH];
static vec2 Bottom[RETRO_WIDTH];
static vec2 Left[RETRO_HEIGHT];
static vec2 Right[RETRO_HEIGHT];
static int StageIndex = 0;
static double StageTime = 0;               // the time the stage started

// count points round the rippled circle, turning a quarter turn from angle start
static void TraceEdge(vec2 *edge, int count, float start, float turn, const Stage *stage, float depth, float roll)
{
	for (int i = 0; i < count; i++) {
		float a = start + turn * i / count;
		float r = stage->radius + depth * sinf(a * stage->waves + roll);
		edge[i] = vec2{ RETRO_WIDTH / 2.0f, RETRO_HEIGHT / 2.0f } + r * vec2{ cosf(a), sinf(a) };
	}
}

static void DrawPicture(void)
{
	const unsigned char *image = RETRO_ImageData();
	unsigned char *dst = RETRO_FrameBuffer();
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float s = (float)y / RETRO_HEIGHT;

		// The side edges' bulge off the straight lines between the corners
		vec2 left = Left[y] - (Top[0] + (Bottom[0] - Top[0]) * s);
		vec2 right = Right[y] - (Top[RETRO_WIDTH - 1] + (Bottom[RETRO_WIDTH - 1] - Top[RETRO_WIDTH - 1]) * s);
		vec2 rowstep = (right - left) / RETRO_WIDTH;

		// Each column runs from its top edge's point to its bottom's
		for (int x = 0; x < RETRO_WIDTH; x++) {
			vec2 p = Top[x] + (Bottom[x] - Top[x]) * s + left + rowstep * x + PICTURE_OFFSET;
			*dst++ = image[WRAP256(p.y) * 256 + WRAP256(p.x)];
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	// Move on to the next stage when this one is over
	float t = time.total - StageTime;
	if (t >= Stages[StageIndex].duration) {
		StageTime += Stages[StageIndex].duration;
		StageIndex = (StageIndex + 1) % STAGE_COUNT;
		t = time.total - StageTime;
	}
	const Stage *stage = &Stages[StageIndex];

	// Fade in and out
	RETRO_Fade(MIN(t, stage->duration - t) / FADE_TIME, RETRO_ImagePalette());

	// Turn the circle and roll its ripple, swinging the depth if the stage does
	float angle = START_ANGLE + radians(t * stage->spin);
	float roll = radians(t * stage->roll);
	float depth = stage->depth + stage->swing * sinf(t * SWING_SPEED);

	// The border's points round the circle, a quarter turn an edge
	TraceEdge(Top, RETRO_WIDTH, angle, QUARTER_TURN, stage, depth, roll);
	TraceEdge(Bottom, RETRO_WIDTH, angle - QUARTER_TURN, -QUARTER_TURN, stage, depth, roll);
	TraceEdge(Right, RETRO_HEIGHT, angle + QUARTER_TURN, QUARTER_TURN, stage, depth, roll);
	TraceEdge(Left, RETRO_HEIGHT, angle, -QUARTER_TURN, stage, depth, roll);

	DrawPicture();
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage(PICTURE);
}
