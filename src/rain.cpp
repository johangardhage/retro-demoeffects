//
// Rain
//
// Rain slanting down over a city at night, splashing into the flooded
// street in front of it, with lightning now and then.
//
// A drop has a depth z in [0, 1], far to near. It falls at
//
//   vy = RAIN_MINSPEED + z (RAIN_MAXSPEED − RAIN_MINSPEED),  vx = RAIN_SLANT vy
//
// and is drawn as a streak back along its velocity, RAIN_STREAK seconds of
// travel long, so near drops are long bright streaks and far ones short
// dim ticks. The street is a plane seen from above its surface, whose rows
// run from STREET_TOP at the far edge to the bottom of the screen at the
// near one, so a drop lands on the row that stands for its depth,
//
//   landing y = STREET_TOP + z (H − STREET_TOP)
//
// and there starts a ripple, an ellipse flattened by the same perspective
// that grows and fades over RIPPLE_LIFE. Drops nearer than SPLASH_DEPTH
// also throw up SPLASH_DROPLETS droplets that fly under gravity for a
// moment.
//
// The city is a row of dark blocks with scattered lit windows, drawn once.
// Lightning strikes at random, every LIGHTNING_INTERVAL seconds on
// average, as a double flash: every palette color is mixed toward the
// flash's color by a brightness that jumps up twice and decays in between,
// times a weight per color. The sky takes all of it, the street and the
// rain some, and the city almost none, so it stands out black against the
// lit sky.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define DROPS 500
#define RAIN_MINSPEED 220.0f // pixels a second, farthest layer
#define RAIN_MAXSPEED 520.0f // nearest layer
#define RAIN_SLANT 0.18 // sideways pixels per pixel of fall
#define RAIN_STREAK 0.03 // seconds of travel a streak spans
#define STREET_TOP 176 // the street's far edge, where the city stands
#define RIPPLES 256
#define RIPPLE_LIFE 0.5 // seconds
#define RIPPLE_SIZE 9.0 // pixels across a ripple grows at the near edge
#define RIPPLE_FLATTEN 0.25 // height over width of a ripple
#define SPLASH_DEPTH 0.7 // drops nearer than this splash
#define SPLASH_DROPLETS 3
#define SPLASHES 512
#define SPLASH_SPEED 60.0 // pixels a second, upward at most
#define SPLASH_GRAVITY 500.0 // pixels a second squared
#define LIGHTNING_INTERVAL 7.0 // seconds between strikes, on average
#define LIGHTNING_DECAY 9.0 // how fast a flash fades, per second
#define LIGHTNING_GAP 0.12 // seconds between a strike's two flashes
#define WINDOW_LIT 0.3 // chance a window is lit
#define SKY 0 // palette ranges
#define SKY_COLORS 48
#define STREET (SKY + SKY_COLORS)
#define STREET_COLORS 32
#define BUILDING (STREET + STREET_COLORS)
#define WINDOW (BUILDING + 1)
#define RAIN (WINDOW + 1)
#define RAIN_COLORS 16
#define PALETTE_COLORS (RAIN + RAIN_COLORS)

struct Drop {
	float x;
	float y;
	float z;
};

struct Ripple {
	float x;
	float y;
	float size;
	float age; // seconds, RIPPLE_LIFE or more when unused
};

struct Splash {
	float x;
	float y;
	float vx;
	float vy;
	float floor; // the row it falls back to
	bool alive;
};

static Drop Drops[DROPS];
static Ripple Ripples[RIPPLES];
static Splash Splashes[SPLASHES];
static int NextRipple, NextSplash;
static unsigned char City[RETRO_WIDTH * STREET_TOP]; // the city's palette entries, 0 where the sky shows
static RETRO_Palette Colors[PALETTE_COLORS]; // the palette before lightning
static float Lit[PALETTE_COLORS]; // how much of a flash each color takes
static double Flash; // brightness of the lightning, 0 to 1

//
// Start a drop at a random place at its depth, above the top of the screen when fresh
//
static void Respawn(Drop *drop, bool fresh)
{
	drop->x = mix(-RETRO_HEIGHT * RAIN_SLANT, RETRO_WIDTH, RAND());
	drop->y = fresh ? -RANDOMF(RETRO_HEIGHT / 2) : RANDOMF(RETRO_HEIGHT);
}

//
// A drop lands: a ripple where it hits and, up close, a few droplets
//
static void Land(const Drop &drop, float landing)
{
	Ripple *ripple = &Ripples[NextRipple];
	NextRipple = (NextRipple + 1) % RIPPLES;
	ripple->x = drop.x;
	ripple->y = landing;
	ripple->size = RIPPLE_SIZE * (0.25f + 0.75f * drop.z);
	ripple->age = 0;

	if (drop.z >= SPLASH_DEPTH) {
		for (int i = 0; i < SPLASH_DROPLETS; i++) {
			Splash *splash = &Splashes[NextSplash];
			NextSplash = (NextSplash + 1) % SPLASHES;
			splash->x = drop.x;
			splash->y = landing - 1;
			splash->vx = mix(-1, 1, RAND()) * SPLASH_SPEED * 0.6f;
			splash->vy = -SPLASH_SPEED * mix(0.4, 1.0, RAND());
			splash->floor = landing;
			splash->alive = true;
		}
	}
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	// Move the drops, landing them on the street at their depth
	for (Drop &drop : Drops) {
		float speed = mix(RAIN_MINSPEED, RAIN_MAXSPEED, drop.z);
		drop.x += speed * RAIN_SLANT * time.delta;
		drop.y += speed * time.delta;

		float landing = mix(STREET_TOP, RETRO_HEIGHT, drop.z);
		if (drop.y >= landing) {
			Land(drop, landing);
			Respawn(&drop, true);
		}
	}

	// Age the ripples
	for (Ripple &ripple : Ripples) {
		ripple.age += time.delta;
	}

	// Fly the droplets until they fall back
	for (Splash &splash : Splashes) {
		if (splash.alive) {
			splash.vy += SPLASH_GRAVITY * time.delta;
			splash.x += splash.vx * time.delta;
			splash.y += splash.vy * time.delta;
			splash.alive = splash.y < splash.floor;
		}
	}

	// Strike lightning now and then, as a double flash
	static double sincestrike = 1e9;
	sincestrike += time.delta;
	if (RAND() < time.delta / LIGHTNING_INTERVAL) {
		sincestrike = 0;
	}
	Flash *= exp(-LIGHTNING_DECAY * time.delta);
	if (sincestrike < time.delta) {
		Flash = 1.0;
	} else if (sincestrike >= LIGHTNING_GAP && sincestrike < LIGHTNING_GAP + time.delta) {
		Flash = 0.8;
	}
}

//
// Plot a pixel if it is on the screen
//
static void Plot(unsigned char *buffer, int x, int y, unsigned char color)
{
	if (RETRO_OnScreen(x, y)) {
		buffer[y * RETRO_WIDTH + x] = color;
	}
}

void DEMO_Render(RETRO_Time time)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Mix the palette toward the lightning
	for (int i = 0; i < PALETTE_COLORS; i++) {
		float flash = Flash * Lit[i];
		RETRO_SetColor(i, mix(Colors[i], RETRO_Palette{ 215, 220, 255 }, flash));
	}

	// Draw sky and city, then the street
	for (int y = 0; y < STREET_TOP; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char city = City[y * RETRO_WIDTH + x];
			buffer[y * RETRO_WIDTH + x] = city ? city : SKY + y * SKY_COLORS / STREET_TOP;
		}
	}
	for (int y = STREET_TOP; y < RETRO_HEIGHT; y++) {
		memset(buffer + y * RETRO_WIDTH, STREET + (y - STREET_TOP) * STREET_COLORS / (RETRO_HEIGHT - STREET_TOP), RETRO_WIDTH);
	}

	// Draw ripples, rings that grow and fade
	for (Ripple &ripple : Ripples) {
		if (ripple.age < RIPPLE_LIFE) {
			float k = ripple.age / RIPPLE_LIFE;
			float ra = ripple.size * (0.2f + 0.8f * k) / 2;
			float rb = ra * RIPPLE_FLATTEN;
			unsigned char color = RAIN + (int)((1 - k) * (RAIN_COLORS - 1) * 0.7f);
			int points = MAX((int)(2 * M_PI * ra), 6);
			for (int i = 0; i < points; i++) {
				float a = 2 * M_PI * i / points;
				Plot(buffer, (int)floorf(ripple.x + ra * cosf(a)), (int)floorf(ripple.y + rb * sinf(a)), color);
			}
		}
	}

	// Draw the streaks, far to near so the near ones cross in front
	for (Drop &drop : Drops) {
		float speed = mix(RAIN_MINSPEED, RAIN_MAXSPEED, drop.z);
		float length = speed * RAIN_STREAK;
		unsigned char color = RAIN + MIN((int)(drop.z * RAIN_COLORS), RAIN_COLORS - 1);
		int x1 = (int)floorf(drop.x);
		int y1 = (int)floorf(drop.y);
		int x2 = (int)floorf(drop.x - length * RAIN_SLANT);
		int y2 = (int)floorf(drop.y - length);
		if (y1 >= 0 && y2 < RETRO_HEIGHT) {
			RETRO_DrawLine(x1, y1, x2, y2, color);
		}
	}

	// Draw the droplets
	for (Splash &splash : Splashes) {
		if (splash.alive) {
			Plot(buffer, (int)floorf(splash.x), (int)floorf(splash.y), RAIN + RAIN_COLORS - 1);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. A cloudy night sky, a wet street lit from the city, dark
	// blocks with warm windows and the rain from gray-blue to pale blue
	RETRO_CreateGradientPalette(SKY, SKY + SKY_COLORS, { 12, 14, 26 }, { 46, 44, 58 }, Colors);
	RETRO_CreateGradientPalette(STREET, STREET + STREET_COLORS, { 30, 32, 46 }, { 10, 12, 20 }, Colors);
	Colors[BUILDING] = { 8, 9, 14 };
	Colors[WINDOW] = { 150, 120, 60 };
	RETRO_CreateGradientPalette(RAIN, RAIN + RAIN_COLORS, { 52, 58, 78 }, { 170, 185, 215 }, Colors);

	// The flash lights the sky fully, the street and rain partly, and leaves
	// the city a silhouette against it
	for (int i = 0; i < PALETTE_COLORS; i++) {
		Lit[i] = i < STREET ? 1.0f : i < BUILDING ? 0.45f : i < RAIN ? 0.03f : 0.6f;
	}

	// Init city, blocks side by side with windows lit at random
	int x = 0;
	for (int i = 0; x < RETRO_WIDTH; i++) {
		int width = 14 + RANDOM(24);
		int height = 30 + RANDOM(70) + (i % 3 == 1 ? 30 : 0);
		int top = STREET_TOP - height;
		for (int y = top; y < STREET_TOP; y++) {
			for (int xx = x; xx < MIN(x + width, RETRO_WIDTH); xx++) {
				City[y * RETRO_WIDTH + xx] = BUILDING;
			}
		}
		for (int wy = top + 3; wy + 2 <= STREET_TOP; wy += 5) {
			for (int wx = x + 2; wx + 2 <= MIN(x + width - 2, RETRO_WIDTH); wx += 4) {
				if (RAND() < WINDOW_LIT) {
					for (int y = wy; y < wy + 2; y++) {
						memset(City + y * RETRO_WIDTH + wx, WINDOW, 2);
					}
				}
			}
		}
		x += width + RANDOM(3);
	}

	// Init drops, spread over the screen so the rain has already started. Each
	// keeps its depth, rising along the array, so drawing in order is far to near
	for (int i = 0; i < DROPS; i++) {
		Drops[i].z = (i + RANDOMF(1)) / DROPS;
		Respawn(&Drops[i], false);
	}
	for (Ripple &ripple : Ripples) {
		ripple.age = RIPPLE_LIFE;
	}
}
