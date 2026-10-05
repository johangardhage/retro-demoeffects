//
// Page curl
//
// One picture peels off the other from the bottom-right corner, rolling over
// a cylinder of radius R. The fold line is every screen point p with
//
//   d = p · n − c = 0
//
// n is the unit direction the corner lifts along and c moves the line from
// beyond the bottom-right corner to beyond the top-left one. A page point s
// past the line (s = its own p · n − c) sits on the paper, rolled, at
//
//   s ≤ 0        d = s,                     z = 0      flat, front up
//   0 ≤ s ≤ πR   d = R sin(s/R),            z = R (1 − cos(s/R))
//   s ≥ πR       d = πR − s,                z = 2R     flat, back up
//
// and keeps its coordinate along the line. The view is straight down, so a
// screen pixel at d is covered by up to three page points, and the highest
// one whose source lies on the page wins:
//
//   back    s = πR − d (d < 0),  or  s = R (π − asin(d/R)) (0 ≤ d ≤ R)
//   front   s = R asin(d/R) (0 ≤ d ≤ R),  or  s = d (d < 0)
//
// The source pixel is p + (s − d) n. Anything left uncovered is the picture
// underneath. The paper at s has turned through θ = s/R, so its front normal
// is (−sin θ, cos θ) in (n, z), and it is lit by Lambert from LIGHT, scaled
// so that flat paper keeps its colors. The back side shows the same pixel
// mirrored, washed toward PAPER. The picture underneath darkens under the
// roll and for SHADOW_WIDTH pixels past it.
//
// Both pictures share one palette, so the shaded RGB is mapped back to it
// through a 32³ inverse color LUT. The pictures take turns being on top.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define CURL_RADIUS 18.0f     // pixels
#define CURL_DIRECTION_X 0.8f // n, the direction the corner lifts along
#define CURL_DIRECTION_Y 0.6f
#define LIGHT_N -0.5f         // light, along n and up; it falls from the top left
#define LIGHT_Z 1.0f
#define AMBIENT 0.35f         // light a face turned away from LIGHT still gets
#define PAPER RETRO_Palette{ 235, 230, 215 }
#define BACK_TINT 0.75f       // how far the back side is washed toward PAPER
#define SHADOW_WIDTH 24.0f    // pixels past the roll the shadow fades over
#define SHADOW_DEPTH 0.55f    // fraction of the light the shadow takes away
#define TIME_HOLD 1.0         // seconds a picture lies flat
#define TIME_CURL 3.0         // seconds a picture takes to peel off
#define TIME_TURN (TIME_HOLD + TIME_CURL)

static RETRO_Image *Picture[2];
static unsigned char ColorLUT[32][32][32];

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase. Each turn one picture peels off the other
	double phase = fmod(time.total, 2 * TIME_TURN);
	int top = step(TIME_TURN, phase);
	double turntime = phase - top * TIME_TURN;
	float progress = smoothstep(0.0, 1.0, (turntime - TIME_HOLD) / TIME_CURL);

	RETRO_Palette *palette = Picture[0]->palette;
	unsigned char *front = Picture[top]->data;
	unsigned char *under = Picture[1 - top]->data;
	unsigned char *buffer = RETRO_FrameBuffer();

	// The fold line runs from just past the far corner, where nothing is lifted
	// yet, to where even the flap turned back over it has left the screen
	vec2 n = normalize(vec2{ CURL_DIRECTION_X, CURL_DIRECTION_Y });
	float r = CURL_RADIUS;
	float cstart = RETRO_WIDTH * n.x + RETRO_HEIGHT * n.y + 1;
	float cend = -M_PI * r - 1;
	float c = mix(cstart, cend, progress);

	float lightlength = hypotf(LIGHT_N, LIGHT_Z);
	float lightn = LIGHT_N / lightlength;
	float lightz = LIGHT_Z / lightlength;

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float d = (x + 0.5f) * n.x + (y + 0.5f) * n.y - c;

			// The page points over this pixel, highest first: the back side,
			// then the front under it. Past the roll there are none
			float candidate[2];
			int candidates = 0;
			if (d <= r) {
				float arc = d < 0 ? d : r * asin(d / r);
				candidate[0] = M_PI * r - arc;
				candidate[1] = arc;
				candidates = 2;
			}
			float red = 0, green = 0, blue = 0;
			bool covered = false;

			for (int i = 0; i < candidates && !covered; i++) {
				float s = candidate[i];
				bool back = i == 0;
				int sx = floor(x + (s - d) * n.x);
				int sy = floor(y + (s - d) * n.y);

				if (!RETRO_OnScreen(sx, sy)) {
					continue;
				}

				// Lambert on the side that faces up, relative to flat paper
				float theta = s <= 0 ? 0 : (s >= M_PI * r ? M_PI : s / r);
				float lambert = -sin(theta) * lightn + cos(theta) * lightz;
				lambert = back ? -lambert : lambert;
				float shade = MIN(1.0f, mix(AMBIENT, 1, MAX(0.0f, lambert) / lightz));

				RETRO_Palette color = palette[front[sy * RETRO_WIDTH + sx]];
				red = color.r;
				green = color.g;
				blue = color.b;

				if (back) {
					red = mix(red, PAPER.r, BACK_TINT);
					green = mix(green, PAPER.g, BACK_TINT);
					blue = mix(blue, PAPER.b, BACK_TINT);
				}

				red *= shade;
				green *= shade;
				blue *= shade;
				covered = true;
			}

			if (!covered) {
				// The picture underneath, in the shadow of the roll
				float shadow = d <= r ? 1 : CLAMP01(1 - (d - r) / SHADOW_WIDTH);
				float shade = 1 - SHADOW_DEPTH * shadow;

				RETRO_Palette color = palette[under[y * RETRO_WIDTH + x]];
				red = color.r * shade;
				green = color.g * shade;
				blue = color.b * shade;
			}

			buffer[y * RETRO_WIDTH + x] = ColorLUT[(int)red >> 3][(int)green >> 3][(int)blue >> 3];
		}
	}
}

void DEMO_Initialize(void)
{
	Picture[0] = RETRO_LoadImage("assets/monkey_320x240_quantizized.pcx", true);
	Picture[1] = RETRO_LoadImage("assets/flowers_320x240_quantizized.pcx");

	// The pictures share this palette, so one LUT maps every shaded color back
	RETRO_CreateColorLUT(Picture[0]->palette, 32, &ColorLUT[0][0][0]);
}
