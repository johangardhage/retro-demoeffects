//
// Pseudo 3D road 2
//
// The road with a sky and distance fog, drawn the way the arcade machines
// drew it: one scanline at a time, bottom to top, with no per-pixel and no
// per-quad divide anywhere in the frame. Every scanline below the
// horizon is a distance, and those distances are a table built once.
// Inverting the projection of the ground plane, a camera at
// ROAD_CAMERA_HEIGHT looking at row H/2 + i sees
//
//   z = D h (H/2) / i,  w = D ROAD_WIDTH (W/2) / z
//
// so ZMap holds the distance a row looks at and WidthMap the half-width of
// the road there, both in the order the beam draws them. Drawing is then a
// walk up the screen reading the table, and the road ends where the table
// runs past ROAD_DRAW_DISTANCE rather than at a far clip plane.
//
// Nothing knows where the road is. The centre is accumulated up the screen
// from the bottom, where it is under the camera, by the two step recurrence
//
//   x += dx,  dx += segment.curve
//
// which traces a parabola in screen space. This is the fake: the curve is not
// a place the road bends, it is an acceleration applied per scanline, so the
// bend belongs to the rows and not to the world. A bend far ahead sits in the
// few top rows where one row spans hundreds of world units and barely tilts
// them; as it comes closer it takes more rows, and the road leans into it.
//
// Hills warp the table instead of the road. The walk holds a fractional index
// and steps it by 1 + segment.hill rather than by 1, so a hill is a rate and
// not an acceleration: it belongs to the distance the row looks at, and it
// bends only the rows that look that far. A step under one reads a row of the
// table twice and stretches the road up the screen, which is a climb; a step
// over one skips rows and packs distance into fewer of them, which runs the
// table out early and drops the road away over a crest. The step is floored
// above zero, since at zero the walk would read one distance on every row and
// stand a single slice of road up the whole screen, and below it the walk
// would go back down the table; the hills here stay well clear of that floor,
// so it guards the constants rather than shapes the effect. It costs an add,
// and it is a warp, not geometry: the horizon never moves, and the camera can
// neither climb nor look down. That is the trade the technique makes, and
// what a projected engine buys with its divides.
//
// Because the walk visits every row once and each row is one memset and three
// spans, the road costs a screen, whatever the track does. Fog is a shade
// picked per row from that row's z, so it grades smoothly rather than in
// bands, and the sky ends on the same haze the ramps start at, which is what
// hides the row where the table runs out.
//
// Roadside billboards ride the same walk. A segment stepped over on the way
// up stands between the row that stepped over it and the row below, and it is
// placed by how far between the two its own distance falls. The rows either
// side carry the centre of the road and its half-width, so the billboard
// takes its position and its scale from the same interpolation and needs no
// projection of its own. Reading the row it landed on instead would cost
// nothing and look far worse: one row up here spans thousands of world units,
// so a billboard would hold a size for half a second and then jump a fifth of
// it at once. They are collected going up, which is near to far, and drawn
// afterwards in the order they were found reversed, so a near one paints over
// a far one. A crest clips them for nothing: the segments past it are never
// walked into, so their billboards are never collected. What the walk cannot
// place is a billboard nearer than the bottom row, since no row looks that
// close; it is dropped, and with billboards standing this far out it has left
// the side of the screen long before.
//
// Stripes come from the distance too: the tarmac, the rumble and the grass
// switch between two colors every ROAD_STRIPE_LENGTH of world z, so they flow
// through the perspective on their own. The track holds a whole number of
// stripe pairs, so they still alternate where it loops.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define ROAD_MAX_SEGMENTS 2048
#define ROAD_SEGMENT_LENGTH 200 // world units of z one segment of track lasts
#define ROAD_DRAW_DISTANCE 60000 // world units the table is allowed to reach
#define ROAD_WIDTH 2000 // half the width of the tarmac, in world units
#define ROAD_LANES 3
#define ROAD_LANE_WIDTH 0.025 // half a lane marking, as a fraction of the half-width
#define ROAD_RUMBLE_WIDTH 0.35 // how far the rumble strip reaches outside the tarmac
#define ROAD_STRIPE_LENGTH 600 // world units one stripe lasts
#define ROAD_CAMERA_HEIGHT 1300 // world units above the road, and it stays there
#define ROAD_FIELD_OF_VIEW 100 // degrees across the screen
#define ROAD_SPEED 11000 // world units a second
#define ROAD_CENTRIFUGAL 12.0 // half-widths a second the camera is pushed out per unit of curve
#define ROAD_RECENTER 1.1 // how quickly it eases back to the middle
#define ROAD_MIN_STEP 0.15 // the slowest step the walk may take, for hills steeper than these
#define ROAD_SPRITE_SIZE 32 // the largest billboard bitmap, either way
#define ROAD_MAX_BILLBOARDS 320 // one per segment the draw distance can hold
#define ROAD_FOG_DENSITY 8.0 // set so the haze closes over the road before the table runs out

#define ROAD_HORIZON (RETRO_HEIGHT / 2)
#define ROAD_ZMAP_SIZE ROAD_HORIZON // one entry per row the ground can be seen on
#define ROAD_CAMERA_DEPTH (1.0 / tan((ROAD_FIELD_OF_VIEW / 2.0) * DEG2RAD))

// Curve is an acceleration, in pixels across per scanline squared; hill is a
// rate, the table entries a scanline takes on top of the one it would take on
// the flat. A section eases in and out of the values it is given
#define ROAD_CURVE_EASY 0.012
#define ROAD_CURVE_MEDIUM 0.025
#define ROAD_CURVE_HARD 0.040
#define ROAD_HILL_LOW 0.12
#define ROAD_HILL_HIGH 0.20

// The palette holds one ramp per surface, haze to surface, and the sky above them
#define ROAD_SHADES 24
#define ROAD_MATERIALS 8
#define ROAD_SKY (ROAD_MATERIALS * ROAD_SHADES)
#define ROAD_SKY_SHADES (RETRO_COLORS - ROAD_SKY)

enum { MATERIAL_GRASS_DARK, MATERIAL_GRASS_LIGHT, MATERIAL_RUMBLE_DARK, MATERIAL_RUMBLE_LIGHT,
	MATERIAL_TARMAC_DARK, MATERIAL_TARMAC_LIGHT, MATERIAL_LANE, MATERIAL_TREE };

#define ROAD_FOG RETRO_HAZE
#define ROAD_ZENITH RETRO_SPACECADET

static const RETRO_Palette MaterialColor[ROAD_MATERIALS] = {
	RETRO_HUNTERGREEN, RETRO_FORESTGREEN, RETRO_FIREBRICK, RETRO_LIGHTGRAY,
	RETRO_CHARCOAL, RETRO_DIMGRAY, RETRO_LIGHTGRAY, RETRO_PINETREE };

enum { SPRITE_TREE, SPRITE_POLE, ROAD_SPRITES };

struct Sprite {
	unsigned char pixel[ROAD_SPRITE_SIZE * ROAD_SPRITE_SIZE]; // a material one higher, or 0 for transparent
	int width;
	int height;
	float size; // how wide the billboard stands in the world, in road half-widths
};

// A billboard the walk has come across, kept until the road is drawn
struct Billboard {
	Sprite *sprite;
	float x; // where it stands, in pixels
	float y; // the row it stands on
	float w; // the half-width of the road there, which is also its scale
	int shade;
};

struct Segment {
	float curve; // pixels across the road centre gains per scanline, per scanline
	float hill; // table entries a scanline takes on top of the one the flat road takes
	int sprite; // an index into Sprites, or -1
	float offset; // where the sprite stands, in half-widths from the centre
};

Sprite Sprites[ROAD_SPRITES];
Segment Road[ROAD_MAX_SEGMENTS];
Billboard Billboards[ROAD_MAX_BILLBOARDS];
int RoadSegments = 0;

float ZMap[ROAD_ZMAP_SIZE]; // the distance the row looks at, nearest first
float WidthMap[ROAD_ZMAP_SIZE]; // half the road, in pixels, at that distance
unsigned char FogMap[ROAD_ZMAP_SIZE]; // the shade that distance is seen through
int ZMapEntries = 0; // the entries inside the draw distance, so the walk stops at one test

//
// Fill a scanline from x1 to x2, clipped to the screen. The ends are pixel
// positions, not columns, so a span narrower than a pixel drops out
//
void DrawSpan(unsigned char *row, float x1, float x2, unsigned char color)
{
	int left = x1 < 0 ? 0 : (int)x1;
	int right = x2 > RETRO_WIDTH ? RETRO_WIDTH : (int)x2;

	if (right > left) {
		memset(row + left, color, right - left);
	}
}

//
// Draw a billboard standing at (x, y) on a row whose road half-width is w,
// which is the only scale it needs, and fogged to that row's shade. The
// destination pixels drive the sampling, so a sprite that fills the screen
// costs a screen, not a bitmap
//
void DrawSprite(unsigned char *dest, Sprite *sprite, float x, float y, float w, int shade)
{
	float width = sprite->size * w;
	float height = width * sprite->height / sprite->width;
	if (width < 1 || height < 1) {
		return;
	}

	float left = x - width / 2;
	float top = y - height;
	int x1 = MAX((int)left, 0);
	int x2 = MIN((int)(left + width), RETRO_WIDTH);
	int y1 = MAX((int)top, 0);
	int y2 = MIN((int)(top + height), RETRO_HEIGHT);

	for (int sy = y1; sy < y2; sy++) {
		int v = CLAMP((sy - top) * sprite->height / height, 0, sprite->height);
		for (int sx = x1; sx < x2; sx++) {
			int u = CLAMP((sx - left) * sprite->width / width, 0, sprite->width);
			unsigned char texel = sprite->pixel[v * sprite->width + u];
			if (texel) {
				dest[sy * RETRO_WIDTH + sx] = (texel - 1) * ROAD_SHADES + shade;
			}
		}
	}
}

void DEMO_Render(double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();

	// Drive. The track loops, so the distance travelled wraps at its length
	double tracklength = RoadSegments * (double)ROAD_SEGMENT_LENGTH;
	static double position = 0;
	position = fmod(position + deltatime * ROAD_SPEED, tracklength);

	// A bend pushes the camera toward the outside of it, and the straight
	// pulls it back to the middle
	int base = (int)(position / ROAD_SEGMENT_LENGTH) % RoadSegments;
	static float playerx = 0;
	playerx += deltatime * (-Road[base].curve * ROAD_CENTRIFUGAL - playerx * ROAD_RECENTER);

	// Sky. t = 0 at the zenith and 1 at the horizon, and t² packs the haze
	// into the rows above it. Below the horizon the haze is left standing,
	// so the row where the table runs out has nothing to run out against
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float t = CLAMP01((float)y / ROAD_HORIZON);
		memset(dest + y * RETRO_WIDTH, ROAD_SKY + (int)(t * t * (ROAD_SKY_SHADES - 1)), RETRO_WIDTH);
	}

	// Walk up the screen. x is where the centre of the road has reached and
	// dx how fast it is moving sideways, zi how far into the table the walk
	// has come. The camera's own place across the road is not part of that
	// walk: it is worth playerx half-widths at every row, which is a wide
	// shift near the camera and almost none at the far end, so weaving moves
	// the road under the camera and leaves the vanishing point where it is.
	// Neither x nor dx survives the frame
	float x = RETRO_WIDTH / 2.0f;
	float dx = 0;
	float zi = 0;

	// Billboards are gathered on the way up, which is near to far, each one
	// placed between the row that stepped over it and the row below. The walk
	// starts a row under the bottom of the screen, so the first row has a row
	// below it to place against
	int billboards = 0;
	int walked = (int)(position / ROAD_SEGMENT_LENGTH); // the segment under the camera
	float zprev = 2 * ZMap[0] - ZMap[1];
	float wprev = 2 * WidthMap[0] - WidthMap[1];
	float centreprev = RETRO_WIDTH / 2.0f - playerx * wprev;
	float yprev = RETRO_HEIGHT;

	for (int y = RETRO_HEIGHT - 1; y >= ROAD_HORIZON; y--) {
		int entry = (int)zi;
		if (entry >= ZMapEntries) {
			break; // the table has run past the draw distance, and the road with it
		}

		float z = ZMap[entry];
		float w = WidthMap[entry];
		int shade = FogMap[entry];

		// The stripes belong to the world, not to the rows, so they are read
		// off the distance the row is looking at
		int light = (int)((position + z) / ROAD_STRIPE_LENGTH) & 1;
		unsigned char grass = (light ? MATERIAL_GRASS_LIGHT : MATERIAL_GRASS_DARK) * ROAD_SHADES + shade;
		unsigned char rumble = (light ? MATERIAL_RUMBLE_LIGHT : MATERIAL_RUMBLE_DARK) * ROAD_SHADES + shade;
		unsigned char tarmac = (light ? MATERIAL_TARMAC_LIGHT : MATERIAL_TARMAC_DARK) * ROAD_SHADES + shade;

		// Where the road is drawn on this row, once the camera's place across
		// it is taken off at this row's scale
		float centre = x - playerx * w;

		unsigned char *row = dest + y * RETRO_WIDTH;
		memset(row, grass, RETRO_WIDTH);
		DrawSpan(row, centre - w * (1 + ROAD_RUMBLE_WIDTH), centre + w * (1 + ROAD_RUMBLE_WIDTH), rumble);
		DrawSpan(row, centre - w, centre + w, tarmac);

		// Lane markings live on the light stripes only, which is what leaves
		// a gap between one dash and the next
		if (light) {
			for (int i = 1; i < ROAD_LANES; i++) {
				float lx = centre - w + 2 * w * i / ROAD_LANES;
				DrawSpan(row, lx - w * ROAD_LANE_WIDTH, lx + w * ROAD_LANE_WIDTH, MATERIAL_LANE * ROAD_SHADES + shade);
			}
		}

		// The segment this row is looking at accelerates the centre and sets
		// how much of the table the next row takes
		int number = (int)((position + z) / ROAD_SEGMENT_LENGTH); // counted along the drive, so it only grows
		Segment *segment = &Road[number % RoadSegments];

		// A far row can step over several segments at once, so none of their
		// billboards is missed, and each is placed by its own distance rather
		// than by the row that found it
		while (walked < number && billboards < ROAD_MAX_BILLBOARDS) {
			Segment *stepped = &Road[++walked % RoadSegments];
			if (stepped->sprite >= 0) {
				float zs = walked * (double)ROAD_SEGMENT_LENGTH - position;
				if (zs < zprev) {
					continue; // nearer than the bottom row, so the walk has no row to stand it on
				}

				float t = (zs - zprev) / (z - zprev);
				float sw = wprev + (w - wprev) * t;
				float sx = centreprev + (centre - centreprev) * t + stepped->offset * sw;

				Billboards[billboards++] = { &Sprites[stepped->sprite], sx, yprev + (y - yprev) * t, sw, shade };
			}
		}

		zprev = z;
		wprev = w;
		centreprev = centre;
		yprev = y;

		dx += segment->curve;
		x += dx;
		zi += MAX(1 + segment->hill, (float)ROAD_MIN_STEP);
	}

	// The walk found them near to far, so they are drawn the other way about
	// and a near billboard paints over the one behind it
	while (billboards--) {
		Billboard *billboard = &Billboards[billboards];
		DrawSprite(dest, billboard->sprite, billboard->x, billboard->y, billboard->w, billboard->shade);
	}
}

//
// A section reaching curve and hill over its first segments, holding them, and
// letting them go again, so the road never breaks where one section meets the
// next. Curve is an acceleration and hill a rate, but they are eased the same
// way regardless: what a seam needs is that neither arrives at a value the
// section before it did not already hold
//
void AddRoad(int enter, int hold, int leave, float curve, float hill)
{
	int total = enter + hold + leave;

	for (int n = 0; n < total; n++) {
		if (RoadSegments >= ROAD_MAX_SEGMENTS) {
			RETRO_RageQuit("Too many road segments\n");
		}

		// How far into the run in, and into the run out, this segment sits
		float in = n < enter ? (float)n / enter : 1.0f;
		float out = n < enter + hold ? 0.0f : (float)(n - enter - hold) / leave;
		float ease = in * in * (1 + cos(out * M_PI)) / 2;

		Segment *segment = &Road[RoadSegments++];
		segment->curve = curve * ease;
		segment->hill = hill * ease;
		segment->sprite = -1;
		segment->offset = 0;
	}
}

void DEMO_Initialize(void)
{
	// Init palette. One ramp per surface, the haze the sky ends at up to the
	// surface itself, so distance is a shade of the surface's own hue
	for (int m = 0; m < ROAD_MATERIALS; m++) {
		RETRO_CreateGradientPalette(m * ROAD_SHADES, (m + 1) * ROAD_SHADES, ROAD_FOG, MaterialColor[m]);
		RETRO_SetColor((m + 1) * ROAD_SHADES - 1, MaterialColor[m]);
	}

	RETRO_CreateGradientPalette(ROAD_SKY, ROAD_SKY + ROAD_SKY_SHADES / 2, ROAD_ZENITH, RETRO_GLAUCOUS);
	RETRO_CreateGradientPalette(ROAD_SKY + ROAD_SKY_SHADES / 2, RETRO_COLORS, RETRO_GLAUCOUS, ROAD_FOG);
	RETRO_SetColor(RETRO_COLORS - 1, ROAD_FOG);

	// Init the tables, nearest row first. Row H/2 + i looks at the ground
	// z = D h (H/2) / i, and the half row keeps the horizon itself out of the
	// table, where i is zero and the distance is not a number. Entries past
	// the draw distance are built but never walked into
	for (int i = 0; i < ROAD_ZMAP_SIZE; i++) {
		float rows = ROAD_ZMAP_SIZE - i - 0.5f; // rows below the horizon, nearest first
		float z = ROAD_CAMERA_DEPTH * ROAD_CAMERA_HEIGHT * ROAD_HORIZON / rows;
		float t = z / ROAD_DRAW_DISTANCE;

		ZMap[i] = z;
		WidthMap[i] = ROAD_CAMERA_DEPTH * ROAD_WIDTH * (RETRO_WIDTH / 2.0f) / z;
		FogMap[i] = CLAMP(exp(-ROAD_FOG_DENSITY * t * t) * ROAD_SHADES, 0, ROAD_SHADES);

		if (z <= ROAD_DRAW_DISTANCE) {
			ZMapEntries = i + 1;
		}
	}

	// Init track. The stripes are read off the world, so the track holds a
	// whole number of stripe pairs and they alternate across the seam too
	AddRoad(40, 60, 40, 0, 0);
	AddRoad(40, 40, 40, ROAD_CURVE_MEDIUM, ROAD_HILL_LOW);
	AddRoad(40, 40, 40, 0, -ROAD_HILL_LOW);
	AddRoad(50, 50, 50, -ROAD_CURVE_HARD, 0);
	AddRoad(30, 60, 30, 0, ROAD_HILL_HIGH);
	AddRoad(40, 40, 40, ROAD_CURVE_EASY, -ROAD_HILL_HIGH);
	AddRoad(60, 60, 60, -ROAD_CURVE_EASY, ROAD_HILL_LOW);
	AddRoad(40, 50, 40, ROAD_CURVE_HARD, -ROAD_HILL_LOW);
	AddRoad(50, 80, 50, 0, 0);

	if ((RoadSegments * ROAD_SEGMENT_LENGTH) % (2 * ROAD_STRIPE_LENGTH)) {
		RETRO_RageQuit("The track does not hold whole stripe pairs\n");
	}

	// Init sprites. Poles line the road at a fixed spacing, alternating sides,
	// and trees stand further out wherever a segment has no pole
	for (int i = 0; i < RoadSegments; i++) {
		if (i % 10 == 0) {
			Road[i].sprite = SPRITE_POLE;
			Road[i].offset = (i % 20) ? 1.6 : -1.6;
		} else if (RANDOM(5) == 0) {
			Road[i].sprite = SPRITE_TREE;
			Road[i].offset = (RANDOM(2) ? 1 : -1) * (2.0 + RANDOMF(3.0));
		}
	}

	// A tree is a cone of foliage on a dark trunk
	Sprite *tree = &Sprites[SPRITE_TREE];
	tree->width = 24;
	tree->height = 32;
	tree->size = 0.75;

	int canopy = tree->height * 3 / 4; // the rows the foliage covers, the rest is trunk

	for (int y = 0; y < tree->height; y++) {
		for (int x = 0; x < tree->width; x++) {
			float half = y < canopy ? 1 + (tree->width / 2.0f - 1) * y / (canopy - 1) : 1.5f;
			int material = y < canopy ? MATERIAL_TREE : MATERIAL_TARMAC_DARK;
			bool inside = fabs(x - (tree->width - 1) / 2.0f) <= half;
			tree->pixel[y * tree->width + x] = inside ? material + 1 : 0;
		}
	}

	// A marker pole is banded, so the bands scale away with it
	Sprite *pole = &Sprites[SPRITE_POLE];
	pole->width = 4;
	pole->height = 24;
	pole->size = 0.07;

	for (int y = 0; y < pole->height; y++) {
		int material = (y / 6) & 1 ? MATERIAL_RUMBLE_DARK : MATERIAL_RUMBLE_LIGHT;
		memset(pole->pixel + y * pole->width, material + 1, pole->width);
	}
}
