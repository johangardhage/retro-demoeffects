//
// Explode
//
// A picture that blows apart into its pixels, which fly out, hang and come
// back down as another picture, then blow apart and fly back.
//
// Every pixel of either picture is a particle, and particle i leaves pixel
// a_i of one picture for pixel b_i of the other. The pairing is by
// brightness, made within each PAIR_BLOCK square: the square's pixels in
// both pictures are shuffled and then sorted by the luminance of their
// color, and the i-th of one list is paired with the i-th of the other. Dark
// lands on dark and light on light, so the colors change little on the way,
// and no particle lands far from where it would have, so the blast keeps its
// shape all the way down. Along its flight a particle's color is blended
// from its first to its second in RGB and looked up in an inverse color
// table of the palette the two pictures share.
//
// Each flight has its own origin, a random point on the picture, and the blast
// spreads out from it: a particle sets off after a delay in proportion to its
// distance from the origin. It lands at a moment of its own, drawn at random
// from the last part of the flight, so the picture does not close in a wave
// behind the blast but fills in everywhere at once, pixel by pixel. Between the
// two, t runs from 0 to 1. It is carried from a to b by a smoothstep, and blown
// outward, away from the origin, by a burst
//
//   p(t) = a + (b − a) (3t² − 2t³) + d · 27/4 t (1 − t)²
//
// The burst leaves at full speed and is back to nothing at t = 1, with its
// peak |d| at t = 1/3, so the particle is flung out, hangs and settles on its
// pixel with no speed left. d points away from the origin, turned a little
// either way, with a length of its own.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retromath.h"

#define PARTICLES (RETRO_WIDTH * RETRO_HEIGHT)
#define HOLD_TIME 2.0 // seconds a picture stands whole
#define FLIGHT_TIME 4.0 // seconds from one picture to the other
#define BLAST_SPREAD 0.35 // of the flight, the delay of the particles farthest from the origin
#define LAND_SPREAD 0.3 // of the flight, the part at the end the particles land at random in
#define BLAST_DISTANCE 160.0 // pixels a particle is thrown out at the most
#define BLAST_TURN 0.6 // radians either way a particle strays from straight out
#define PAIR_BLOCK 16 // pixels across the squares the pairing is made within, dividing the screen
#define LUT_SIZE 32 // cells a channel of the inverse color table

struct Particle {
	short ax, ay; // pixel in the first picture
	short bx, by; // pixel in the second
	unsigned char colora, colorb;
	float strength; // of BLAST_DISTANCE
	float turn; // of BLAST_TURN, from -1 to 1
	float landing; // of LAND_SPREAD, how early it lands
};

static Particle Particles[PARTICLES];
static unsigned char ColorLUT[LUT_SIZE][LUT_SIZE][LUT_SIZE];
static unsigned char Background; // the palette's nearest to black

//
// The addresses of a block of the picture's pixels, shuffled, then sorted by
// the luminance of their color, darkest first
//
static void SortPixels(const RETRO_Image *picture, const int *rank, int blockx, int blocky, int *pixels)
{
	// Shuffle the addresses, so pixels of one color are paired at random
	int shuffled[PAIR_BLOCK * PAIR_BLOCK];
	for (int i = 0; i < PAIR_BLOCK * PAIR_BLOCK; i++) {
		shuffled[i] = (blocky * PAIR_BLOCK + i / PAIR_BLOCK) * RETRO_WIDTH + blockx * PAIR_BLOCK + i % PAIR_BLOCK;
	}
	for (int i = PAIR_BLOCK * PAIR_BLOCK - 1; i > 0; i--) {
		int j = RANDOM(i + 1);
		SWAP(shuffled[i], shuffled[j]);
	}

	// Counting sort them by rank, which keeps the shuffle within a rank
	int start[RETRO_COLORS + 1] = { 0 };
	for (int address : shuffled) {
		start[rank[picture->data[address]] + 1]++;
	}
	for (int i = 0; i < RETRO_COLORS; i++) {
		start[i + 1] += start[i];
	}
	for (int address : shuffled) {
		pixels[start[rank[picture->data[address]]]++] = address;
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	RETRO_Clear(Background);

	// Find the flight, its direction and its origin
	int flight = (int)floor(time / (HOLD_TIME + FLIGHT_TIME));
	double progress = (fmod(time, HOLD_TIME + FLIGHT_TIME) - HOLD_TIME) / FLIGHT_TIME;
	bool back = flight & 1;
	float originx = RETRO_Hash(flight, 0) % RETRO_WIDTH;
	float originy = RETRO_Hash(flight, 1) % RETRO_HEIGHT;
	float farthest = hypotf(RETRO_WIDTH, RETRO_HEIGHT);
	RETRO_Palette *palette = RETRO_ImagePalette();

	for (Particle &particle : Particles) {
		float fromx = back ? particle.bx : particle.ax, fromy = back ? particle.by : particle.ay;
		float tox = back ? particle.ax : particle.bx, toy = back ? particle.ay : particle.by;
		unsigned char from = back ? particle.colorb : particle.colora;
		unsigned char to = back ? particle.colora : particle.colorb;

		// The particle's own time, from its departure, delayed by its distance
		// from the origin, to its landing
		float dx = fromx - originx, dy = fromy - originy;
		float distance = length(vec2{ dx, dy });
		float departure = BLAST_SPREAD * distance / farthest;
		float landing = 1 - LAND_SPREAD * particle.landing;
		float t = CLAMP01((float)(progress - departure) / (landing - departure));
		if (t <= 0) {
			buffer[(int)fromy * RETRO_WIDTH + (int)fromx] = from;
			continue;
		}
		if (t >= 1) {
			buffer[(int)toy * RETRO_WIDTH + (int)tox] = to;
			continue;
		}

		// Carry it across and blow it outward
		float carry = smoothstep(0.0f, 1.0f, t);
		float burst = 27.0f / 4 * t * (1 - t) * (1 - t);
		float angle = atan2f(dy, dx) + BLAST_TURN * particle.turn;
		float reach = BLAST_DISTANCE * particle.strength * burst;
		int x = (int)floorf(mix(fromx, tox, carry) + reach * cosf(angle));
		int y = (int)floorf(mix(fromy, toy, carry) + reach * sinf(angle));
		if (x < 0 || x >= RETRO_WIDTH || y < 0 || y >= RETRO_HEIGHT) {
			continue;
		}

		// Blend its color on the way
		RETRO_Palette ca = palette[from], cb = palette[to];
		int r = (int)(mix(ca.r, cb.r, carry) + 0.5f);
		int g = (int)(mix(ca.g, cb.g, carry) + 0.5f);
		int b = (int)(mix(ca.b, cb.b, carry) + 0.5f);
		buffer[y * RETRO_WIDTH + x] = ColorLUT[r * LUT_SIZE / 256][g * LUT_SIZE / 256][b * LUT_SIZE / 256];
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *picturea = RETRO_LoadImage("assets/monkey_320x240_quantizized.pcx", true);
	RETRO_Image *pictureb = RETRO_LoadImage("assets/flowers_320x240_quantizized.pcx");
	if (picturea->width != RETRO_WIDTH || picturea->height != RETRO_HEIGHT || pictureb->width != RETRO_WIDTH || pictureb->height != RETRO_HEIGHT) {
		RETRO_RageQuit("Explode pictures must be %dx%d\n", RETRO_WIDTH, RETRO_HEIGHT);
	}
	RETRO_CreateColorLUT(picturea->palette, LUT_SIZE, &ColorLUT[0][0][0]);
	Background = RETRO_NearestPaletteIndex(RETRO_BLACK, picturea->palette);

	// Rank the shared palette by luminance
	int order[RETRO_COLORS], rank[RETRO_COLORS];
	float luminance[RETRO_COLORS];
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_Palette color = picturea->palette[i];
		luminance[i] = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
		int j = i;
		for (; j > 0 && luminance[order[j - 1]] > luminance[i]; j--) {
			order[j] = order[j - 1];
		}
		order[j] = i;
	}
	for (int i = 0; i < RETRO_COLORS; i++) {
		rank[order[i]] = i;
	}

	// Pair the pictures' pixels by brightness, block by block
	static int pixelsa[PARTICLES], pixelsb[PARTICLES];
	for (int blocky = 0; blocky < RETRO_HEIGHT / PAIR_BLOCK; blocky++) {
		for (int blockx = 0; blockx < RETRO_WIDTH / PAIR_BLOCK; blockx++) {
			int first = (blocky * RETRO_WIDTH / PAIR_BLOCK + blockx) * PAIR_BLOCK * PAIR_BLOCK;
			SortPixels(picturea, rank, blockx, blocky, pixelsa + first);
			SortPixels(pictureb, rank, blockx, blocky, pixelsb + first);
		}
	}
	for (int i = 0; i < PARTICLES; i++) {
		Particle &particle = Particles[i];
		particle.ax = pixelsa[i] % RETRO_WIDTH;
		particle.ay = pixelsa[i] / RETRO_WIDTH;
		particle.bx = pixelsb[i] % RETRO_WIDTH;
		particle.by = pixelsb[i] / RETRO_WIDTH;
		particle.colora = picturea->data[pixelsa[i]];
		particle.colorb = pictureb->data[pixelsb[i]];
		particle.strength = mix(0.3, 1.0, RAND());
		particle.turn = mix(-1, 1, RAND());
		particle.landing = RANDOMF(1);
	}
}
