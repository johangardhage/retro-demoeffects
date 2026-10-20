//
// Flower
//
// "RETRO" over four flowers in turn, each turning and breathing. The text is
// the VGA font drawn TEXT_SCALE times its size, and the flower's skin is
// flower_256x256.pcx.
//
// Each flower is a map from the screen into the skin, built once at the
// start. Rays go out from the middle, one for each of RAYS angles, a pixel a
// step. A ray is the skin's row at its angle, ANGLES_PER_ROW rays to a row, so
// the skin goes twice around. It starts at the skin's column inner, the
// petal's depth at its angle, and reaches out a column a pixel to SKIN_EDGE,
// or the screen's edge, past where the skin's petals end, so the petals are
// never cut. SKIN_EDGE leaves room for the breath, so a ray never reads into
// the next row. A ray never covers the text, and a later ray covers an
// earlier one. The pixels no ray reaches show the black behind.
//
// Every frame the flower turns by moving the rows it reads, and it breathes by
// moving the columns back and forth. Both run on the demo's clock, so each
// flower comes in at a different point of its turn and breath.
//
// The skin's colors are four ramps of the same sixteen, each pulsing in
// brightness a quarter turn after the one before. The text's ramp pulses with
// the first. A flower fades in, holds, fades out and stays black for the rest
// of its time, while the text stays.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"

constexpr int FLOWERS = 4;
constexpr double FLOWER_TIME = 6.3;        // seconds a flower lasts
constexpr double FADE_TIME = 0.9;          // seconds it fades in and out over
constexpr double FADE_OUT = 4.2;           // seconds in that it starts to fade out

constexpr int CENTER_X = RETRO_WIDTH / 2;
constexpr int CENTER_Y = RETRO_HEIGHT / 2;
constexpr int RAYS = 4096;
constexpr int ANGLES_PER_ROW = 8;          // rays that read the same row of the skin
constexpr unsigned short NO_FLOWER = 0xffff;
constexpr int TEXT_SCALE = 4;
constexpr int TEXT_COLOR = 97;             // a stone gray in the text's ramp

constexpr double TURN_SPEED = 140.0;       // rows of the skin the flower turns a second
constexpr int BREATH_COLUMNS = 32;         // columns it breathes across
constexpr double BREATH_RATE = 1.1;        // breaths a second
constexpr int SKIN_EDGE = 256 - BREATH_COLUMNS; // the column a ray ends short of

constexpr int RAMP_COLORS = 16;
constexpr int TEXT_RAMP = 96;              // the text's ramp's first color
constexpr int SKIN_RAMP = 16;              // and the skin's first ramp's
constexpr int SKIN_RAMPS = 4;
constexpr double PULSE_RATE = 1.7;         // pulses a second
constexpr double PULSE_LOW = 0.5;          // the darkest it pulses to, of its full brightness

// A flower, its petals and how deep they cut
struct Flower {
	int petals;
	int depth;
};

static const Flower Flowers[FLOWERS] = {
	{ 3, 32 },
	{ 13, 32 },
	{ 3, 64 },
	{ 4, 64 },
};

static unsigned char Text[RETRO_WIDTH * RETRO_HEIGHT];
static unsigned short FlowerMap[FLOWERS][RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char *Skin;
static RETRO_Palette Palette[RETRO_COLORS];

// The text in TEXT_COLOR, centred on the flower
static void DrawText(const char *text)
{
	RETRO_Image *image = RETRO_GenerateTextImage(RETRO_LoadFont(RETRO_FONT_VGA_8X8), text, TEXT_SCALE);
	int x = CENTER_X - image->width / 2;
	int y = CENTER_Y - image->height / 2;
	for (int j = 0; j < image->height; j++) {
		for (int i = 0; i < image->width; i++) {
			if (image->data[j * image->width + i]) {
				Text[(y + j) * RETRO_WIDTH + x + i] = TEXT_COLOR;
			}
		}
	}
}

// The rays from the middle, each one the skin's row at its angle, its
// columns from inner on, one a pixel
static void BuildFlower(const Flower &flower, unsigned short *map)
{
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		map[i] = NO_FLOWER;
	}

	for (int angle = 0; angle < RAYS; angle++) {
		double a = 2 * M_PI * angle / RAYS;
		int inner = (int)((sin(a * flower.petals) + 1) * flower.depth / 2) + 1;
		int row = (angle / ANGLES_PER_ROW) & 255;
		for (int column = inner; column < SKIN_EDGE; column++) {
			double radius = column - inner + 1;
			int x = (int)floor(CENTER_X + radius * cos(a));
			int y = (int)floor(CENTER_Y + radius * sin(a));
			if (!RETRO_OnScreen(x, y)) {
				break;
			}
			int pixel = y * RETRO_WIDTH + x;
			if (Text[pixel] == 0) {
				map[pixel] = (unsigned short)(row << 8 | column);
			}
		}
	}
}

// The ramp's colors at their brightness a phase of turns into the pulse
static void ShadeRamp(int first, double phase)
{
	float brightness = (float)(PULSE_LOW + (1 - PULSE_LOW) * (sin(2 * M_PI * phase) + 1) / 2);
	for (int i = first; i < first + RAMP_COLORS; i++) {
		Palette[i] = Palette[i] * brightness;
	}
}

void DEMO_Render(RETRO_Time time)
{
	double seconds = fmod(time.total, FLOWERS * FLOWER_TIME);
	int index = (int)(seconds / FLOWER_TIME);
	double local = seconds - index * FLOWER_TIME;

	// The flower turned and breathed. The turn moves the rows and the breath
	// the columns, both added to where the map reads
	int turn = (int)(time.total * TURN_SPEED) & 255;
	int breath = (int)((sin(2 * M_PI * time.total * BREATH_RATE) + 1) * BREATH_COLUMNS / 2);
	unsigned short offset = (unsigned short)(turn << 8 | breath);

	unsigned char *dst = RETRO_FrameBuffer();
	const unsigned short *map = FlowerMap[index];
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		dst[i] = map[i] == NO_FLOWER ? Text[i] : Skin[(unsigned short)(map[i] + offset)];
	}

	// Each ramp pulses a quarter turn after the one before, the text's with
	// the first
	memcpy(Palette, RETRO_ImagePalette(), sizeof(Palette));
	double pulse = time.total * PULSE_RATE;
	ShadeRamp(TEXT_RAMP, pulse);
	for (int ramp = 0; ramp < SKIN_RAMPS; ramp++) {
		ShadeRamp(SKIN_RAMP + ramp * RAMP_COLORS, pulse + (double)ramp / SKIN_RAMPS);
	}

	// Fade in, hold, fade out and stay black, all but the text
	RETRO_Fade((float)(MIN(local, FADE_OUT + FADE_TIME - local) / FADE_TIME), Palette);
	RETRO_SetColor(TEXT_COLOR, Palette[TEXT_COLOR]);
}

void DEMO_Initialize(void)
{
	Skin = RETRO_LoadImage("assets/flower_256x256.pcx")->data;
	DrawText("RETRO");
	for (int i = 0; i < FLOWERS; i++) {
		BuildFlower(Flowers[i], FlowerMap[i]);
	}
}
