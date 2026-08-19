//
// Wormhole
//
// A precomputed map from screen pixel to a 15×15 texture. Ring d and spoke s
// land at
//
//   (x, y) = (xd cos θ, yd sin θ) + (W/2, H/4 - z)
//   xd = W d / DIVS,   yd = H d / DIVS
//   z  = Z0 + ZLOG * log(2 d / DIVS)
//
// The ellipse grows linearly with d. The log in z packs far rings toward
// the throat. The texture index is (s/8, d/7) wrapped to 15. Each frame the
// tiny texture is scrolled; φ lives on 15. The map is only a lookup.
//
// The map is built by inverting that, one screen pixel at a time. Writing
// D = d / DIVS, the two components say
//
//   (x − W/2) / W          = D cos θ
//   (y − H/4 + z(D)) / H   = D sin θ
//
// so the angle drops out of the sum of squares and D is the root of
//
//   D² = a² + (c + k ln 2D)²,   a = (x − W/2)/W,
//                               c = (y − H/4 + Z0)/H,  k = ZLOG/H
//
// then θ = atan2 of the two components. Substituting D = e^t/2 turns the log
// into t and makes the equation smooth in t; the derivative
// e^{2t}/2 − 2k(c + kt) is positive over the outer range, so Newton run down
// from t = ln 2 lands on the largest root.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define WORM_XDIR -1
#define WORM_YDIR 1
#define WORM_SPOKES 2400 // angular cells the spoke index is quantised to
#define WORM_DIVS 2400 // ring cells the ring index is quantised to
#define WORM_Z0 -10 // constant offset of the throat
#define WORM_ZLOG 11 // how hard the log packs far rings
#define WORM_SPEED 100 // texture texels a second
#define TEXTURE_WIDTH 15
#define TEXTURE_HEIGHT 15

unsigned char WormHole[RETRO_WIDTH * RETRO_HEIGHT];

void DEMO_Render(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * WORM_SPEED, TEXTURE_WIDTH);
	int xphase = (int)phase * WORM_XDIR;
	int yphase = (int)phase * WORM_YDIR;

	unsigned char newimage[TEXTURE_WIDTH * TEXTURE_HEIGHT];

	// Create new image
	for (int y = 0; y < TEXTURE_HEIGHT; y++) {
		for (int x = 0; x < TEXTURE_WIDTH; x++) {
			newimage[TEXTURE_WIDTH * y + x] = image[TEXTURE_WIDTH * WRAP(y + yphase, TEXTURE_HEIGHT) + WRAP(x + xphase, TEXTURE_WIDTH)];
		}
	}

	// Draw wormhole
	unsigned char *buffer = RETRO_FrameBuffer();
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		buffer[i] = newimage[WormHole[i]];
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/wormhole_15x15.pcx", true);

	// Init wormhole. One solve per screen pixel; see the note at the top.
	double k = (double)WORM_ZLOG / RETRO_HEIGHT;
	double tmax = log(2.0);
	double tmin = log(2.0 / WORM_DIVS);

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double a = (x - RETRO_WIDTH / 2.0) / RETRO_WIDTH;
			double c = (y - RETRO_HEIGHT / 4.0 + WORM_Z0) / RETRO_HEIGHT;

			// Newton on D² − a² − (c + k ln 2D)², in t = ln 2D, from the outermost
			// ring down to the largest root
			double t = tmax;
			for (int i = 0; i < 40; i++) {
				double b = c + k * t;
				double e = exp(2 * t);
				double f = e / 4 - a * a - b * b;
				double df = e / 2 - 2 * k * b;
				if (fabs(df) < 1.0e-14) break;
				double next = CLAMP01((t - f / df - tmin) / (tmax - tmin)) * (tmax - tmin) + tmin;
				if (fabs(next - t) < 1.0e-13) {
					t = next;
					break;
				}
				t = next;
			}

			double b = c + k * t;
			double distance = exp(t) / 2;
			double angle = atan2(b, a);
			if (angle < 0) {
				angle += 2 * M_PI;
			}

			// A pixel outside the outermost ellipse has no root and takes the rim
			int d = CLAMP((int)(distance * WORM_DIVS), 1, WORM_DIVS);
			int s = WRAP((int)(angle / (2 * M_PI) * WORM_SPOKES), WORM_SPOKES);

			WormHole[y * RETRO_WIDTH + x] = WRAP(s / 8, TEXTURE_WIDTH) + (TEXTURE_WIDTH * WRAP(d / 7, TEXTURE_WIDTH));
		}
	}
}
