//
// Techno bars
//
// A set of BARS parallel bars, as wide as the gaps between them, turns about
// its middle while it is squeezed together and let out again. It is drawn
// LAYERS times over, each time as it was LAYER_DELAY before the time before,
// and each time into a bit plane of its own, so a pixel's color number has
// one bit for every layer that has a bar on it. The palette only counts the
// bits: black where there are none, and brighter for every layer more. Where
// the layers have turned away from each other their bars cross, and the
// crossings line up into the diamonds and bands of a moiré, coarse while the
// layers are nearly in step and finer the further apart they are.
//
// A pixel is on a bar of a layer when, measured across the bars from the
// middle of the set,
//
//   u   = (p - middle) · (cos angle, -sin angle)
//   bar = round(u / pitch),  |bar| ≤ BARS / 2,  |u - bar pitch| < pitch / 4
//
// with pitch = BAR_PITCH spread. The bars are endless along their length.
//
// The motion comes in sequences that follow each other and then start over.
// In each the bars start at an angle and a rate of turning, and the rate may
// grow steadily, so that the layers, which are a fixed time apart, come to be
// further and further apart in angle. The spread is a ball let go: it falls
// from the spread it starts at under a gravity, bounces off a floor without
// loss and is back where it started, over and over. In the last sequence the
// middle of the set also circles the middle of the screen. A sequence starts
// with empty bit planes, so a layer is not drawn until the sequence is as old
// as the layer is late.
//
// Every BEAT_TIME the palette is flashed, the brighter the redder, and fades
// back over FLASH_TIME.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define LAYERS 4
#define LAYER_DELAY (8.0 / 70) // seconds a layer is behind the one before it
#define BARS 11
#define BAR_PITCH 64.0f // pixels from the middle of a bar to the next, at a spread of 1
#define ORBIT_SPEED 7.3 // radians per second the middle of the set circles at
#define BEAT_TIME 0.5 // seconds from a flash to the next
#define FLASH_TIME 0.2 // seconds a flash takes to fade

struct Sequence {
	double duration; // seconds
	double angle; // radians the bars are turned by at the start
	double spin; // radians per second they turn at then
	double spinup; // and what that grows by every second
	double spread; // the spread the bars are let go at
	double floor; // the spread they bounce at
	double gravity; // spread per second gained every second on the way down
	float orbit; // pixels from the middle of the screen to the middle of the set
};

static const Sequence Sequences[] = {
	{ 6, 0.28, 0.86, 0, 0.5, 0.25, 49, 0 },
	{ 12, 0.31, 0.43, 3.0, 1, 0, 0.77, 0 },
	{ 14, 0.28, 0.43, 3.0, 1, 0, 0.77, 64 },
};

// The color of a pixel with bars of no layers on it, of one, and so on
static const RETRO_Palette Levels[LAYERS + 1] = {
	{ 0, 0, 0 }, { 85, 77, 101 }, { 117, 101, 134 }, { 154, 142, 170 }, { 190, 178, 206 },
};

// What a flash at its brightest adds to red, green and blue, as a part of each
static const float Flash[3] = { 1.5f, 1.1f, 0.8f };

struct Layer {
	bool drawn;
	float x, y; // the middle of the set
	float acrossx, acrossy; // the unit vector across the bars
	float pitch;
};

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase: the sequence, and how long it has run
	double cycle = 0;
	for (const Sequence &sequence : Sequences) {
		cycle += sequence.duration;
	}
	double phase = fmod(time.total, cycle);
	int iphase = 0;
	while (phase >= Sequences[iphase].duration) {
		phase -= Sequences[iphase].duration;
		iphase++;
	}
	const Sequence &sequence = Sequences[iphase];

	// Flash the palette. Color number bits is the layers with a bar on the pixel
	float flash = MAX(1 - fmod(time.total, BEAT_TIME) / FLASH_TIME, 0);
	for (int bits = 0; bits < 1 << LAYERS; bits++) {
		int count = 0;
		for (int i = 0; i < LAYERS; i++) {
			count += bits >> i & 1;
		}
		RETRO_Palette level = Levels[count];
		RETRO_SetColor(bits,
			MIN((int)(level.r * (1 + Flash[0] * flash)), 255),
			MIN((int)(level.g * (1 + Flash[1] * flash)), 255),
			MIN((int)(level.b * (1 + Flash[2] * flash)), 255));
	}

	// Place the layers, each as the set was a delay before the one before it
	Layer layers[LAYERS];
	for (int i = 0; i < LAYERS; i++) {
		double t = phase - i * LAYER_DELAY;

		// The spread falls for falltime, bounces, and is back after as long again
		double falltime = sqrt(2 * (sequence.spread - sequence.floor) / sequence.gravity);
		double fallen = fmod(t + falltime, 2 * falltime) - falltime;
		double spread = sequence.spread - sequence.gravity * fallen * fallen / 2;
		double angle = sequence.angle + sequence.spin * t + sequence.spinup * t * t / 2;

		layers[i].drawn = t >= 0 && spread > 0;
		layers[i].x = RETRO_WIDTH / 2.0f + sequence.orbit * sin(ORBIT_SPEED * t);
		layers[i].y = RETRO_HEIGHT / 2.0f + sequence.orbit * cos(ORBIT_SPEED * t);
		layers[i].acrossx = cos(angle);
		layers[i].acrossy = -sin(angle);
		layers[i].pitch = BAR_PITCH * spread;
	}

	// Draw bars, every layer into its own bit of the pixel
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char bits = 0;

			for (int i = 0; i < LAYERS; i++) {
				if (!layers[i].drawn) {
					continue;
				}
				float u = (x + 0.5f - layers[i].x) * layers[i].acrossx + (y + 0.5f - layers[i].y) * layers[i].acrossy;
				int bar = lroundf(u / layers[i].pitch);

				if (abs(bar) <= BARS / 2 && fabsf(u - bar * layers[i].pitch) < layers[i].pitch / 4) {
					bits |= 1 << i;
				}
			}
			RETRO_PutPixel(x, y, bits);
		}
	}
}
