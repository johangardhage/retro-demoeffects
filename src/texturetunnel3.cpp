//
// Tunnel, additive trails
//
// A 160×100 polar map, looked up into a 256×256 plasma and doubled to
// 320×200. The map is a stack of rings: a ring of radius zr, walked by
// its angle θ, lands on
//
//   x = zr (sin θ − cos θ) + 80
//   y = 0.8 · zr (sin θ + cos θ) + 50
//
// an ellipse of radius zr √2, squashed by 0.8 so it fills the 160×100
// frame. The bracketed pair is a 45° turn of scale √2, so a pixel hands
// its own ring and angle straight back:
//
//   zr = |(dx, dy / 0.8)| / √2
//   θ  = atan2(dx, dy / 0.8) + π/4
//
// for dx, dy measured from the centre (80, 50). A pixel then stores
//
//   u = θ · 256 / 2π                   around the tube
//   v = 1.035 ^ ((zc − zr) / 0.5)      along it
//
// Rings sit 0.5 apart and each one is 3.5% further down the tube than the
// ring outside it, counted from zc, the ring through the frame corner. So
// the corner is depth 1 and the centre, the far end of the tube, is 139.
// v stays well under 192, which leaves the 0..63 plasma sample room to
// add on top without wrapping into dark rings. That sample is added into a
// 160×100 buffer, and that buffer is faded through the shade table below,
// which is a fake motion blur. (u, v) tick by one texel every step.
// Each 160×100 sample is written as a 2×2 block.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_SIZE 256
#define MAP_WIDTH 160
#define MAP_HEIGHT 100
#define MAP_SIZE (MAP_WIDTH * MAP_HEIGHT)
#define TUNNEL_SQUASH 0.8 // y squash that fits a ring to the frame
#define TUNNEL_RING_STEP 0.5 // ring spacing, in ring radius units
#define TUNNEL_RING_GAIN 1.035 // texture rows gained per ring inward

unsigned char Precalc[MAP_SIZE * 2];
unsigned char Plasma[TEXTURE_SIZE * TEXTURE_SIZE];
unsigned char TunnelBuffer[MAP_SIZE];
unsigned char Sin2[256];
unsigned char Cos2[256];
unsigned char Shade[256];
unsigned char ScrollU;
unsigned char ScrollV;

void DEMO_FixedUpdate(double timestep)
{
	// Fade first so the blit shows this step's add, matching the original
	// order: accumulate, display, then fade for the next step. Fading up
	// front is that same cycle entered one phase earlier.
	for (int i = 0; i < MAP_SIZE; i++) {
		TunnelBuffer[i] = Shade[TunnelBuffer[i]];
	}

	for (int i = 0; i < MAP_SIZE; i++) {
		unsigned char u = Precalc[i * 2] + ScrollU;
		unsigned char v = Precalc[i * 2 + 1] + ScrollV;
		unsigned char color = Plasma[(v << 8) | u] + Precalc[i * 2 + 1];
		int sum = TunnelBuffer[i] + color;
		TunnelBuffer[i] = sum > 255 ? 255 : sum;
	}

	ScrollU++;
	ScrollV++;
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	for (int y = 0; y < MAP_HEIGHT; y++) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			unsigned char color = TunnelBuffer[y * MAP_WIDTH + x];
			int dx = x * 2;
			int dy = y * 2;
			buffer[dy * RETRO_WIDTH + dx] = color;
			buffer[dy * RETRO_WIDTH + dx + 1] = color;
			buffer[(dy + 1) * RETRO_WIDTH + dx] = color;
			buffer[(dy + 1) * RETRO_WIDTH + dx + 1] = color;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Exact 6-bit VGA DAC entries: a slow
	// cool grey into white, a fall through green, then lime through orange
	// into white-hot. Additive trails climb it; the far centre sits on the
	// bright end.
	static const unsigned char Pal6[RETRO_COLORS][3] = {
		{  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0 }, {  0,  0,  0 },
		{  0,  0,  0 }, {  1,  1,  1 }, {  1,  1,  1 }, {  1,  1,  1 },
		{  1,  1,  1 }, {  1,  1,  1 }, {  1,  1,  1 }, {  2,  2,  2 },
		{  2,  2,  2 }, {  2,  2,  2 }, {  2,  2,  2 }, {  3,  3,  3 },
		{  3,  3,  3 }, {  3,  3,  3 }, {  3,  3,  3 }, {  4,  4,  4 },
		{  4,  4,  4 }, {  4,  4,  4 }, {  4,  4,  4 }, {  5,  5,  5 },
		{  5,  5,  5 }, {  5,  5,  5 }, {  5,  5,  5 }, {  6,  6,  6 },
		{  6,  6,  6 }, {  6,  6,  6 }, {  6,  6,  6 }, {  7,  7,  7 },
		{  7,  7,  7 }, {  7,  7,  7 }, {  7,  7,  7 }, {  8,  8,  8 },
		{  8,  8,  8 }, {  8,  8,  8 }, {  8,  8,  8 }, {  8,  8,  8 },
		{  9,  9,  9 }, {  9,  9,  9 }, {  9,  9, 10 }, { 10, 10, 10 },
		{ 10, 10, 11 }, { 11, 11, 11 }, { 11, 11, 12 }, { 12, 12, 12 },
		{ 12, 12, 13 }, { 13, 13, 13 }, { 13, 13, 14 }, { 14, 14, 14 },
		{ 14, 14, 15 }, { 14, 14, 15 }, { 15, 15, 16 }, { 15, 15, 16 },
		{ 16, 16, 17 }, { 16, 16, 17 }, { 16, 16, 18 }, { 17, 17, 18 },
		{ 17, 17, 19 }, { 18, 18, 19 }, { 18, 18, 20 }, { 18, 18, 21 },
		{ 19, 19, 21 }, { 19, 19, 22 }, { 20, 20, 22 }, { 20, 20, 23 },
		{ 20, 20, 23 }, { 21, 21, 24 }, { 21, 21, 24 }, { 21, 21, 25 },
		{ 22, 22, 25 }, { 22, 22, 26 }, { 22, 22, 26 }, { 23, 23, 27 },
		{ 23, 23, 27 }, { 23, 23, 28 }, { 24, 24, 28 }, { 24, 24, 29 },
		{ 24, 24, 29 }, { 25, 25, 30 }, { 25, 25, 30 }, { 25, 25, 31 },
		{ 25, 25, 31 }, { 26, 26, 32 }, { 26, 26, 32 }, { 26, 26, 33 },
		{ 27, 27, 33 }, { 27, 27, 34 }, { 27, 27, 34 }, { 28, 28, 35 },
		{ 28, 28, 35 }, { 28, 28, 36 }, { 28, 28, 36 }, { 29, 29, 37 },
		{ 29, 29, 38 }, { 29, 29, 38 }, { 29, 29, 39 }, { 30, 30, 39 },
		{ 30, 30, 40 }, { 30, 30, 40 }, { 30, 30, 41 }, { 31, 31, 41 },
		{ 31, 31, 42 }, { 32, 32, 42 }, { 32, 32, 43 }, { 33, 33, 44 },
		{ 34, 34, 44 }, { 35, 35, 45 }, { 36, 36, 46 }, { 37, 37, 46 },
		{ 38, 38, 47 }, { 39, 39, 48 }, { 39, 39, 48 }, { 40, 40, 49 },
		{ 41, 41, 49 }, { 42, 42, 50 }, { 43, 43, 51 }, { 44, 44, 51 },
		{ 45, 45, 52 }, { 46, 46, 53 }, { 47, 47, 53 }, { 48, 48, 54 },
		{ 49, 49, 55 }, { 50, 50, 55 }, { 51, 51, 56 }, { 52, 52, 57 },
		{ 53, 53, 57 }, { 55, 55, 58 }, { 56, 56, 59 }, { 57, 57, 59 },
		{ 58, 58, 60 }, { 59, 59, 61 }, { 60, 60, 61 }, { 61, 61, 62 },
		{ 63, 63, 63 }, { 60, 62, 60 }, { 58, 61, 58 }, { 56, 60, 56 },
		{ 55, 59, 55 }, { 53, 58, 53 }, { 51, 57, 51 }, { 49, 56, 49 },
		{ 47, 55, 47 }, { 45, 54, 45 }, { 44, 53, 44 }, { 42, 52, 42 },
		{ 40, 51, 40 }, { 39, 50, 39 }, { 37, 49, 37 }, { 36, 48, 36 },
		{ 34, 47, 34 }, { 33, 46, 33 }, { 31, 45, 31 }, { 30, 44, 30 },
		{ 28, 43, 28 }, { 27, 42, 27 }, { 26, 41, 26 }, { 24, 40, 24 },
		{ 23, 39, 23 }, { 22, 38, 22 }, { 21, 37, 21 }, { 19, 37, 19 },
		{ 18, 36, 18 }, { 17, 35, 17 }, { 16, 34, 16 }, { 15, 33, 15 },
		{ 14, 32, 14 }, { 13, 31, 13 }, { 12, 30, 12 }, { 11, 29, 11 },
		{ 10, 28, 10 }, {  9, 27,  9 }, {  9, 26,  9 }, {  8, 25,  8 },
		{  7, 24,  7 }, {  6, 23,  6 }, {  6, 22,  6 }, {  5, 21,  5 },
		{  4, 20,  4 }, {  4, 19,  4 }, {  3, 18,  3 }, {  3, 17,  3 },
		{  2, 16,  2 }, {  2, 15,  2 }, {  1, 14,  1 }, {  1, 13,  1 },
		{  1, 12,  1 }, {  1, 11,  1 }, {  0, 10,  0 }, {  0,  9,  0 },
		{  0,  8,  0 }, {  0,  8,  0 }, {  0,  7,  0 }, {  0,  6,  0 },
		{  0,  5,  0 }, {  0,  7,  0 }, {  1,  9,  0 }, {  3, 12,  0 },
		{  5, 14,  0 }, {  7, 16,  0 }, { 10, 19,  0 }, { 14, 21,  0 },
		{ 18, 23,  1 }, { 22, 25,  1 }, { 27, 28,  1 }, { 30, 29,  1 },
		{ 32, 28,  2 }, { 35, 27,  2 }, { 37, 26,  3 }, { 39, 24,  3 },
		{ 42, 22,  4 }, { 44, 19,  4 }, { 46, 17,  5 }, { 49, 14,  6 },
		{ 49, 16,  7 }, { 50, 18,  7 }, { 50, 20,  8 }, { 51, 22,  9 },
		{ 51, 24, 10 }, { 52, 26, 11 }, { 52, 28, 12 }, { 53, 29, 13 },
		{ 53, 31, 14 }, { 54, 33, 15 }, { 54, 35, 16 }, { 55, 37, 17 },
		{ 55, 39, 18 }, { 56, 41, 19 }, { 56, 42, 20 }, { 57, 44, 21 },
		{ 57, 46, 22 }, { 58, 48, 23 }, { 58, 49, 24 }, { 59, 51, 25 },
		{ 59, 52, 26 }, { 60, 54, 27 }, { 60, 56, 29 }, { 61, 57, 30 },
		{ 61, 59, 31 }, { 62, 60, 32 }, { 62, 61, 33 }, { 63, 63, 35 },
		{ 63, 63, 37 }, { 63, 63, 39 }, { 63, 63, 42 }, { 63, 63, 44 },
		{ 63, 63, 46 }, { 63, 63, 49 }, { 63, 63, 51 }, { 63, 63, 53 },
		{ 63, 63, 55 }, { 63, 63, 58 }, { 63, 63, 60 }, { 63, 63, 63 }
	};
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_Set6bitColor(i, Pal6[i][0], Pal6[i][1], Pal6[i][2]);
	}

	// The fade table. Each step pulls the lingering buffer
	// toward black, a little faster than 5/6 at the bottom of the range.
	static const unsigned char FadeTable[256] = {
		  0,   0,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  12,
		 13,  14,  15,  16,  17,  18,  18,  19,  20,  21,  22,  23,  23,  24,  25,  26,
		 27,  27,  28,  29,  30,  31,  32,  33,  33,  34,  35,  36,  37,  38,  38,  39,
		 40,  41,  42,  43,  43,  44,  45,  46,  47,  48,  48,  49,  50,  51,  52,  52,
		 53,  54,  55,  56,  57,  57,  58,  59,  60,  61,  62,  62,  63,  64,  65,  66,
		 67,  68,  68,  69,  70,  71,  72,  73,  73,  74,  75,  76,  77,  78,  78,  79,
		 80,  81,  82,  83,  83,  84,  85,  86,  87,  88,  88,  89,  90,  91,  92,  93,
		 93,  94,  95,  96,  97,  97,  98,  99, 100, 101, 102, 102, 103, 104, 105, 106,
		107, 107, 108, 109, 110, 111, 112, 112, 113, 114, 115, 116, 117, 117, 118, 119,
		120, 121, 122, 122, 123, 124, 125, 126, 127, 127, 128, 129, 130, 131, 132, 133,
		133, 134, 135, 136, 137, 138, 138, 139, 140, 141, 142, 143, 143, 144, 145, 146,
		147, 148, 148, 149, 150, 151, 152, 153, 153, 154, 155, 156, 157, 158, 158, 159,
		160, 161, 162, 163, 163, 164, 165, 166, 167, 168, 168, 169, 170, 171, 172, 173,
		173, 174, 175, 176, 177, 178, 178, 179, 180, 181, 182, 183, 183, 184, 185, 186,
		187, 188, 188, 189, 190, 191, 192, 192, 193, 194, 195, 196, 197, 197, 198, 199,
		200, 201, 202, 202, 203, 204, 205, 206, 207, 207, 208, 209, 210, 211, 212, 212
	};
	memcpy(Shade, FadeTable, 256);

	// Unsigned full-wave tables, |sin| and |cos| of one turn in 256 steps,
	// amplitude 127. The plasma indexes them as 8-bit wrapping bytes.
	for (int i = 0; i < 256; i++) {
		Sin2[i] = (unsigned char)(fabs(sin(2 * M_PI * i / 256.0)) * 127 + 0.5);
		Cos2[i] = (unsigned char)(fabs(cos(2 * M_PI * i / 256.0)) * 127 + 0.5);
	}

	// Plasma. One 256×256 pass of the original byte-arithmetic generator:
	// two |sin|/|cos| lookups of a mixed x/y index, halved and floored at 64
	// so the field lives in 0..63 and the depth add has room to light it.
	// The two drift counters below are XORed into that index, one stepping
	// by 5 a texel and carrying into the other, so the pattern never repeats
	// across the field.
	unsigned char ropurax = 0;
	unsigned char ropuray = 0;
	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) {
		unsigned char cl = i;
		unsigned char ch = i >> 8;

		unsigned char bl = cl;
		bl += Sin2[bl];
		bl ^= ch;
		bl ^= ropuray;
		unsigned char al = Cos2[bl];

		bl += ch;
		bl += Cos2[bl];
		bl ^= ropurax;
		al += Sin2[bl];

		al >>= 1;
		Plasma[i] = al < 64 ? 0 : al - 64;

		unsigned char next = ropurax + 5;
		if (next < ropurax) {
			ropuray++;
		}
		ropurax = next;
	}

	// Polar map. 160×100, two bytes per pixel (u, v). Each pixel is asked
	// which ring it sits on and at what angle, rather than the rings being
	// painted and the gaps between them filled in. The corner ring carries
	// depth 1, so every pixel is that ring's radius or less.
	double corner = hypot(MAP_WIDTH / 2, (MAP_HEIGHT / 2) / TUNNEL_SQUASH) / M_SQRT2;
	for (int y = 0; y < MAP_HEIGHT; y++) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			double dx = x - MAP_WIDTH / 2;
			double dy = (y - MAP_HEIGHT / 2) / TUNNEL_SQUASH;
			double angle = atan2(dx, dy) + M_PI_4;
			double ring = hypot(dx, dy) / M_SQRT2;
			double depth = pow(TUNNEL_RING_GAIN, (corner - ring) / TUNNEL_RING_STEP);

			int i = y * MAP_WIDTH + x;
			Precalc[i * 2] = WRAP256(angle * TEXTURE_SIZE / (2 * M_PI));
			Precalc[i * 2 + 1] = CLAMP(depth, 1, 256);
		}
	}
}
