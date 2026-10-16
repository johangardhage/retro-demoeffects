//
// Fire clouds
//
// A 160x200 cloud that remembers itself. On every pass, 35 a second, each
// byte is sent through a coordinate table, the table is slid by a spiral, and
// the texture sample found there is mixed back into the byte:
//
//   spiral = (sine(angle) + offset) / 256
//   sample = min((24 * texture + 232 * sample + 63) / 256, 112)
//
// offset loses 65536 a pass, so the spiral walks one texture row a step, and
// the sine bends the walk by a few texels at most, so no pass repeats the one
// before. The mix draws a sample toward the texture and never past 112, which
// is what makes the picture burn in and then hold.
//
// The coordinate table is drawn when the demo starts: rings outward from the
// middle, one texture row each, every ring walked downward through
// RING_ANGLES angles and each sample writing its texel where it lands, the
// last write staying. Away from the middle a row steps two or three texture
// rows. Near the middle many angles share a pixel and the smallest, written
// last, stays, which is what turns the middle into a swirl.
//
// The cloud is doubled across the screen and stretched down its 240 rows. A
// bias of 7, 0, 4 and 1, rotated one entry every row, keeps the doubled cloud
// byte's low nibble clear, and the starfield is added there. The high nibble
// picks one of fifteen bands.
//
// The palette is fifteen bands of the sixteen starfield colors, each with one
// color of the fire ramp. A weight, two a pass up to 4096, trades the two:
// the starfield colors fade out and the fire fades in, each scaled in 256ths
// with the fraction dropped and the two added, so the fire slowly takes over
// the picture.
//
// The sine is 32767 sin(2 pi i / 2048) rounded, read at half the angle. The
// texture is fireclouds_ringmap_256x256.pcx, its bytes already scaled by 7/4
// and clamped to 127.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_SIMULATION_STEP (1.0 / 35.0) // one pass of the cloud

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

constexpr int CLOUD_WIDTH = 160;
constexpr int CLOUD_HEIGHT = 200;          // stretched over the screen's 240 rows
constexpr int CLOUD_SIZE = CLOUD_WIDTH * CLOUD_HEIGHT;
constexpr int TEXTURE_WIDTH = 256;
constexpr double RING_STEP = 0.5914;       // cloud rows from one ring of the table to the next
constexpr double RING_SQUEEZE = 0.7;       // across, for VGA's tall pixels
constexpr int RING_ANGLES = 2048;          // samples around a ring
constexpr int TEXTURE_SIZE = TEXTURE_WIDTH * TEXTURE_WIDTH;
constexpr int MIX_PARTS = 256;             // a pass mixes in 256ths
constexpr int TEXTURE_SHARE = 24;          // the texture's part of each pass, the cloud's the rest
constexpr int MIX_ROUND = 63;              // the pass's rounding
constexpr int CLOUD_MAX = 112;             // the brightest a cloud byte gets
constexpr int SPIRAL_STEP = 35;            // the spiral's turn each pass
constexpr int SINE_PERIOD = 4096;          // angle units in one turn of the spiral's sine
constexpr int SINE_ONE = 32767;            // 1.0 in the sine's fixed point
constexpr int SPIRAL_SHIFT = 8;            // the spiral is in 1/256 texels before the shift
constexpr int BANDS = 15;
constexpr int BAND_COLORS = 16;
constexpr int LEVELS = 256;                // the fades step in 1/256
constexpr int WEIGHT_STOP = 4096;
constexpr int DITHER = 4;                  // the bias repeats every 4 rows and columns

// Black through red and orange into white, 6-bit. A whole band of sixteen
// colors is mixed toward one of these.
static const RETRO_Palette Fire[BANDS] = {
	{0, 0, 0}, {14, 3, 3}, {29, 8, 7}, {39, 15, 10}, {49, 23, 14},
	{59, 33, 19}, {60, 40, 25}, {61, 49, 32}, {62, 56, 39}, {62, 57, 44},
	{62, 59, 50}, {63, 61, 56}, {63, 62, 59}, {63, 63, 63}, {63, 63, 63}
};

// Four pixels are one cloud pair doubled: the left pixel, the left pixel
// again, then the right pixel twice. Each row starts one entry further back.
static const unsigned char Bias[DITHER] = { 7, 0, 4, 1 };

static unsigned char Cloud[CLOUD_SIZE];
static const unsigned char *Texture;
static unsigned short Map[CLOUD_SIZE];
static const unsigned char *Stars;
static RETRO_Palette Base[BAND_COLORS];

// Each band's sixteen starfield colors faded out by the weight and its fire
// color faded in, all 6-bit. Each is scaled in steps of 1/LEVELS with the
// fraction dropped before the two are added, so a band never rounds up.
static void SetPalette(int weight)
{
	int firesteps = weight * LEVELS / WEIGHT_STOP;
	int fadesteps = (WEIGHT_STOP - weight) * LEVELS / WEIGHT_STOP;
	float firelevel = (float)firesteps / LEVELS;
	float fadelevel = (float)fadesteps / LEVELS;
	for (int band = 0; band < BANDS; band++) {
		RETRO_Palette fire = Fire[band] * firelevel;
		for (int color = 0; color < BAND_COLORS; color++) {
			RETRO_Palette c = Base[color] * fadelevel + fire;
			RETRO_Set6bitColor(band * BAND_COLORS + color, c.r, c.g, c.b);
		}
	}
}

// The coordinate table: ring by ring outward, each walked down through its
// angles, every sample writing its texture row and column where it lands.
// The column is rounded halves to even, as the FPU rounds.
static void BuildMap(void)
{
	for (int ring = 0; ring < TEXTURE_WIDTH; ring++) {
		for (int step = RING_ANGLES - 1; step >= 0; step--) {
			double angle = step * (2.0 * M_PI / RING_ANGLES);
			int x = (int)floor(CLOUD_WIDTH / 2.0 - cos(angle) * ring * RING_STEP * RING_SQUEEZE);
			int y = (int)floor(CLOUD_HEIGHT / 2.0 + sin(angle) * ring * RING_STEP);
			if (x >= 0 && x < CLOUD_WIDTH && y >= 0 && y < CLOUD_HEIGHT) {
				int column = (int)lrint((double)step * TEXTURE_WIDTH / RING_ANGLES) % TEXTURE_WIDTH;
				Map[y * CLOUD_WIDTH + x] = ring * TEXTURE_WIDTH + column;
			}
		}
	}
}

// One pass: the weight, two a pass from 1, which stops at WEIGHT_STOP; the
// spiral, a texture row back a pass and bent by the sine of an angle
// SPIRAL_STEP on a pass; and the cloud mixed toward the texture through the
// coordinate table slid by the spiral
void DEMO_FixedUpdate(RETRO_Time time)
{
	static int weight = -1;
	weight = MIN(weight + 2, WEIGHT_STOP);
	SetPalette(weight);

	static unsigned pass; // wraps, at a multiple of everything it drives
	pass++;

	// The sine is read at half the angle, the fraction dropped. Only bits 8
	// to 23 of the spiral reach the table.
	int angle = (int)(pass * SPIRAL_STEP % SINE_PERIOD);
	int halfangle = angle / 2;
	int sine = (int)lround(SINE_ONE * sin(halfangle * (2.0 * M_PI / (SINE_PERIOD / 2.0))));
	unsigned spiral = (sine - pass * (TEXTURE_WIDTH << SPIRAL_SHIFT)) >> SPIRAL_SHIFT;
	for (int i = 0; i < CLOUD_SIZE; i++) {
		int texel = Texture[(Map[i] + spiral) % TEXTURE_SIZE];
		int sample = Cloud[i];
		int value = (TEXTURE_SHARE * texel + (MIX_PARTS - TEXTURE_SHARE) * sample + MIX_ROUND) / MIX_PARTS;
		Cloud[i] = (unsigned char)MIN(value, CLOUD_MAX);
	}
}

void DEMO_Render(RETRO_Time time)
{
	unsigned char *dest = RETRO_FrameBuffer();
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int row = y * RETRO_WIDTH;
		int cloudrow = y * CLOUD_HEIGHT / RETRO_HEIGHT * CLOUD_WIDTH;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// The cloud byte, doubled and biased, picks the band, and the
			// stars the color in it
			int sample = Cloud[cloudrow + x * CLOUD_WIDTH / RETRO_WIDTH];
			int band = (sample * 2 + Bias[WRAP(x - y, DITHER)]) / BAND_COLORS;
			dest[row + x] = (unsigned char)(band * BAND_COLORS + Stars[row + x]);
		}
	}
}

void DEMO_Initialize(void)
{
	Texture = RETRO_LoadImage("assets/fireclouds_ringmap_256x256.pcx")->data;
	RETRO_Image *stars = RETRO_LoadImage("assets/stars_320x240.pcx");
	Stars = stars->data;

	BuildMap();

	for (int i = 0; i < BAND_COLORS; i++) {
		Base[i] = RETRO_To6bitColor(stars->palette[i]);
	}
	SetPalette(0);
}
