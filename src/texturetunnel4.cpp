//
// Tunnel, additive trails
//
// A 160×100 polar map, looked up into the flowers photo from texturetunnel3
// and doubled to 320×200. The map is a stack of rings: a ring of radius zr,
// walked by its angle θ, lands on
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
// v stays well under 192, which leaves the 0..63 luminance sample room to
// add on top without wrapping into dark rings. The photo is converted to
// that luminance at load time, since its palette indices are arbitrary and
// cannot be summed directly. That sample is added into a 160×100 buffer,
// and that buffer is faded by 1/6 each step, which is a fake motion blur.
// (u, v) tick by one texel every step. Each 160×100 sample is written as a
// 2×2 block.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TEXTURE_SIZE 256
#define MAP_WIDTH 160
#define MAP_HEIGHT 100
#define MAP_SIZE (MAP_WIDTH * MAP_HEIGHT)
#define TUNNEL_SQUASH 0.8 // y squash that fits a ring to the frame
#define TUNNEL_RING_STEP 0.5 // ring spacing, in ring radius units
#define TUNNEL_RING_GAIN 1.035 // texture rows gained per ring inward

unsigned char AngleMap[MAP_SIZE];
unsigned char DepthMap[MAP_SIZE];
unsigned char Texture[TEXTURE_SIZE * TEXTURE_SIZE];
unsigned char TunnelBuffer[MAP_SIZE];
unsigned char Scroll;

void DEMO_FixedUpdate(double timestep)
{
	// Fade first so the blit shows this step's add, matching the original
	// order: accumulate, display, then fade for the next step. Fading up
	// front is that same cycle entered one phase earlier; nothing here
	// depends on another pixel, so fade and add can share one pass.
	for (int i = 0; i < MAP_SIZE; i++) {
		unsigned char depth = DepthMap[i];
		unsigned char u = AngleMap[i] + Scroll;
		unsigned char v = depth + Scroll;
		unsigned char color = Texture[(v << 8) | u] + depth;

		TunnelBuffer[i] = CLAMP256(TunnelBuffer[i] * 5 / 6 + color);
	}

	Scroll++;
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
	// Init palette: a cool climb from black to white, a fall through a
	// muted green down to near-black, then a rise through a dusty orange
	// back to white-hot. The waypoints are sampled straight from the
	// original hand-tuned VGA table rather than named "pure" colors, since
	// a plain white-to-green-to-orange-to-white ramp through saturated
	// colors reads as neon; this keeps the same dustier, desaturated cast.
	// Additive trails climb this palette; the far centre sits on the
	// bright end.
	RETRO_CreateGradientPalette(0, 136, RETRO_BLACK, RETRO_WHITE);
	RETRO_CreateGradientPalette(136, 150, RETRO_WHITE, RETRO_PALESAGE);
	RETRO_CreateGradientPalette(150, 170, RETRO_PALESAGE, RETRO_FORESTGREEN);
	RETRO_CreateGradientPalette(170, 196, RETRO_FORESTGREEN, RETRO_MOSSBLACK);
	RETRO_CreateGradientPalette(196, 215, RETRO_MOSSBLACK, RETRO_FIREBRICK);
	RETRO_CreateGradientPalette(215, 243, RETRO_FIREBRICK, RETRO_CREAM);
	RETRO_CreateGradientPalette(243, RETRO_COLORS, RETRO_CREAM, RETRO_WHITE);

	// Texture. The flowers photo, converted to a 0..63 luminance so it fits
	// the same accumulator a plasma field once did: the photo's bytes are
	// palette indices, not brightness, so they cannot be summed as they
	// are, and the load leaves the palette set above active rather than
	// the photo's own.
	RETRO_LoadImage("assets/flowers_256x256.pcx");
	unsigned char *photo = RETRO_ImageData();
	RETRO_Palette *photopalette = RETRO_ImagePalette();
	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; i++) {
		RETRO_Palette color = photopalette[photo[i]];
		int luminance = (color.r * 76 + color.g * 150 + color.b * 29) >> 8;
		Texture[i] = luminance >> 2;
	}

	// Polar map. 160×100, an angle and a depth per pixel. Each pixel is
	// asked which ring it sits on and at what angle, rather than the rings
	// being painted and the gaps between them filled in. The corner ring
	// carries depth 1, so every pixel is that ring's radius or less.
	double halfwidth = MAP_WIDTH / 2.0;
	double halfheight = MAP_HEIGHT / 2.0;
	double corner = hypot(halfwidth, halfheight / TUNNEL_SQUASH) / M_SQRT2;
	for (int y = 0; y < MAP_HEIGHT; y++) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			double dx = x - halfwidth;
			double dy = (y - halfheight) / TUNNEL_SQUASH;
			double angle = atan2(dx, dy) + M_PI_4;
			double ring = hypot(dx, dy) / M_SQRT2;
			double depth = pow(TUNNEL_RING_GAIN, (corner - ring) / TUNNEL_RING_STEP);

			int i = y * MAP_WIDTH + x;
			AngleMap[i] = WRAP256(angle * TEXTURE_SIZE / (2 * M_PI));
			DepthMap[i] = CLAMP(depth, 1, 256);
		}
	}
}
