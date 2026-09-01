//
// Pseudo 3D road 4
//
// The road is a strip of quads, each ROAD_SEGMENT_LENGTH deep, laid end to
// end along z and drawn in perspective. A point at world (x, y, z) seen from
// a camera at (cx, cy, cz) lands at
//
//   s  = D / (z - cz)
//   sx = W/2 + s (x - cx) W/2
//   sy = H/2 - s (y - cy) H/2
//   sw = s ROAD_WIDTH W/2
//
// with D = 1 / tan(fov / 2) the distance to the projection plane, so a wider
// field of view pulls the vanishing point closer and flattens the road. Only
// the segment boundaries are projected; a quad is the band between the two
// boundaries it shares with its neighbours, and the centre and half-width of
// a scanline inside it are linear in y between them. That is the affine
// shortcut the whole effect rests on: the divide happens once per boundary,
// not once per scanline.
//
// A segment does not store where it is, only how the road turns while it
// lasts. Walking outward from the camera, curve is an acceleration:
//
//   x += dx,  dx += segment.curve
//
// so the centre traces a parabola per section and joins smoothly. The camera
// sits between boundaries, and the part of the near segment already behind it
// must not bend the road ahead, so the walk starts at dx = -curve p with p
// the fraction of that segment already travelled. Without it the whole road
// would twitch sideways once per segment.
//
// Hills are real geometry, not a stretched z-map: segment.y is the height the
// road stands at where the segment begins, and the projection turns it into
// sy. The camera rides at ROAD_CAMERA_HEIGHT above the road it is standing
// on, so a crest lifts the horizon and a dip drops it, and the road beyond a
// crest is hidden because the quads are painted back to front. A near quad
// covers a band of scanlines from its far boundary down to its near one, and
// anything it hides projects inside that band.
//
// Every color is a palette ramp of ROAD_SHADES entries running from the haze
// at the far end to the material at the near end, so distance is a shade, not
// an RGB blend, and each material fogs through its own hue. A segment picks
// one shade for all of its scanlines from
//
//   fog = e^(-ROAD_FOG_DENSITY t^2),  t = n / ROAD_DRAW_DISTANCE
//
// which leaves the far segments sitting in the haze the sky ends at, so the
// road does not stop, it dissolves. Rumble strips, tarmac and grass alternate
// between two ramps every ROAD_RUMBLE_LENGTH segments; the count of segments
// is a multiple of that period, so the stripes still alternate across the
// seam where the track loops. Lane markings are drawn on the light segments
// only, which is what makes them dashes rather than lines.
//
// Roadside sprites are billboards scaled by the same s that placed the
// segment they stand on and drawn right after it, so a nearer segment paints
// over one standing behind a crest. Their bitmaps hold a material index one
// higher than the material, leaving 0 for transparent.
//
// The camera weaves: a curve pushes it toward the outside of the bend and it
// eases back to the centre, which is a first order response to segment.curve
// rather than a steering wheel.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define ROAD_MAX_SEGMENTS 2048
#define ROAD_SEGMENT_LENGTH 200 // world units of z one quad spans
#define ROAD_DRAW_DISTANCE 300 // segments drawn ahead of the camera
#define ROAD_WIDTH 2000 // half the width of the tarmac, in world units
#define ROAD_LANES 3
#define ROAD_LANE_WIDTH 0.025 // half a lane marking, as a fraction of the half-width
#define ROAD_RUMBLE_WIDTH 0.35 // how far the rumble strip reaches outside the tarmac
#define ROAD_RUMBLE_LENGTH 3 // segments one stripe lasts
#define ROAD_CAMERA_HEIGHT 1300 // world units above the road under the camera
#define ROAD_FIELD_OF_VIEW 100 // degrees across the screen
#define ROAD_NEAR_CLIP 10 // the nearest z the projection is allowed to divide by
#define ROAD_SPEED 11000 // world units a second
#define ROAD_CENTRIFUGAL 0.06 // half-widths a second the camera is pushed out per unit of curve
#define ROAD_RECENTER 1.1 // how quickly it eases back to the middle
#define ROAD_FOG_DENSITY 5.0
#define ROAD_SPRITE_SIZE 32 // the largest billboard bitmap, either way

#define ROAD_CAMERA_DEPTH (1.0 / tan((ROAD_FIELD_OF_VIEW / 2.0) * DEG2RAD))

// Curve is an acceleration, in world units of x per segment squared, and a
// hill is given in segment lengths of climb over the section it belongs to
#define ROAD_CURVE_EASY 2.0
#define ROAD_CURVE_MEDIUM 4.0
#define ROAD_CURVE_HARD 6.0
#define ROAD_HILL_LOW 20.0
#define ROAD_HILL_HIGH 45.0

// The palette holds one ramp per material, haze to material, and the sky above them
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

struct Segment {
	float curve; // world units of x the centre gains per segment, per segment
	float y; // the height the road stands at where the segment begins
	int sprite; // an index into Sprites, or -1
	float offset; // where the sprite stands, in half-widths from the centre
};

// A projected segment boundary: the centre of the road, the scanline it falls
// on, its half-width in pixels, and the perspective scale that placed it
struct Edge {
	float x;
	float y;
	float w;
	float scale;
};

Sprite Sprites[ROAD_SPRITES];
Segment Road[ROAD_MAX_SEGMENTS];
Edge Edges[ROAD_DRAW_DISTANCE + 1];
int RoadSegments = 0;
float RoadHeight = 0; // the height the track has reached, so a section starts where the last one ended

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
// Draw a billboard standing on the road at (x, y), scaled by the perspective
// scale of the segment it belongs to and fogged to the same shade. The
// destination pixels drive the sampling, so a sprite that fills the screen
// costs a screen, not a bitmap
//
void DrawSprite(unsigned char *dest, Sprite *sprite, float x, float y, float scale, int shade)
{
	float width = sprite->size * ROAD_WIDTH * scale * (RETRO_WIDTH / 2.0f);
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

void DEMO_Render(double time, double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();

	// Drive. The track loops, so the distance travelled wraps at its length
	double tracklength = RoadSegments * (double)ROAD_SEGMENT_LENGTH;
	double position = fmod(time * ROAD_SPEED, tracklength);

	// The segment the camera stands on, and how much of it is behind it
	int base = (int)(position / ROAD_SEGMENT_LENGTH);
	float percent = (float)(position - base * (double)ROAD_SEGMENT_LENGTH) / ROAD_SEGMENT_LENGTH;

	// A bend pushes the camera toward the outside of it, and the straight
	// pulls it back to the middle
	static float playerx = 0;
	playerx += deltatime * (-Road[base].curve * ROAD_CENTRIFUGAL - playerx * ROAD_RECENTER);

	// The camera rides above the road it stands on, which is what lifts the
	// horizon over a crest
	float camerax = playerx * ROAD_WIDTH;
	float cameray = Road[base].y + (Road[(base + 1) % RoadSegments].y - Road[base].y) * percent + ROAD_CAMERA_HEIGHT;
	float depth = ROAD_CAMERA_DEPTH;

	// Project the boundaries, walking outward. x is where the centre of the
	// road has reached and dx how fast it is moving sideways; curve
	// accelerates dx, and the travelled part of the near segment is taken off
	// it so the road ahead does not jump as the camera crosses a boundary
	float x = 0;
	float dx = -Road[base].curve * percent;

	for (int n = 0; n <= ROAD_DRAW_DISTANCE; n++) {
		Segment *segment = &Road[(base + n) % RoadSegments];

		double dz = (base + n) * (double)ROAD_SEGMENT_LENGTH - position;
		float scale = depth / MAX(dz, (double)ROAD_NEAR_CLIP);

		Edges[n].x = RETRO_WIDTH / 2.0f + scale * (x - camerax) * (RETRO_WIDTH / 2.0f);
		Edges[n].y = RETRO_HEIGHT / 2.0f - scale * (segment->y - cameray) * (RETRO_HEIGHT / 2.0f);
		Edges[n].w = scale * ROAD_WIDTH * (RETRO_WIDTH / 2.0f);
		Edges[n].scale = scale;

		x += dx;
		dx += segment->curve;
	}

	// Sky. t = 0 at the zenith and 1 at the middle of the screen, and t²
	// packs the haze into the rows the horizon can reach. Rows below it are
	// left at the haze the road fades into
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float t = CLAMP01(y / (RETRO_HEIGHT / 2.0f));
		memset(dest + y * RETRO_WIDTH, ROAD_SKY + (int)(t * t * (ROAD_SKY_SHADES - 1)), RETRO_WIDTH);
	}

	// Draw the quads back to front, so a near one paints over whatever it hides
	for (int n = ROAD_DRAW_DISTANCE - 1; n >= 0; n--) {
		int index = (base + n) % RoadSegments;
		Segment *segment = &Road[index];

		float t = (float)n / ROAD_DRAW_DISTANCE;
		int shade = CLAMP(exp(-ROAD_FOG_DENSITY * t * t) * ROAD_SHADES, 0, ROAD_SHADES);
		int light = (index / ROAD_RUMBLE_LENGTH) & 1;

		unsigned char grass = (light ? MATERIAL_GRASS_LIGHT : MATERIAL_GRASS_DARK) * ROAD_SHADES + shade;
		unsigned char rumble = (light ? MATERIAL_RUMBLE_LIGHT : MATERIAL_RUMBLE_DARK) * ROAD_SHADES + shade;
		unsigned char tarmac = (light ? MATERIAL_TARMAC_LIGHT : MATERIAL_TARMAC_DARK) * ROAD_SHADES + shade;
		unsigned char lane = MATERIAL_LANE * ROAD_SHADES + shade;

		// The quad spans the scanlines between its far and its near boundary.
		// Both are still projected when the near one is behind the camera,
		// where it lands far below the screen and only the clipping is left
		float ytop = Edges[n + 1].y;
		float ybottom = Edges[n].y;

		if (ybottom > ytop && ybottom > 0 && ytop < RETRO_HEIGHT) {
			float step = 1.0f / (ybottom - ytop);
			int y1 = MAX((int)ytop, 0);
			int y2 = MIN((int)ybottom, RETRO_HEIGHT);

			for (int y = y1; y < y2; y++) {
				// The centre and the half-width are linear in y inside the quad
				float k = (y - ytop) * step;
				float cx = Edges[n + 1].x + (Edges[n].x - Edges[n + 1].x) * k;
				float cw = Edges[n + 1].w + (Edges[n].w - Edges[n + 1].w) * k;

				unsigned char *row = dest + y * RETRO_WIDTH;
				memset(row, grass, RETRO_WIDTH);
				DrawSpan(row, cx - cw * (1 + ROAD_RUMBLE_WIDTH), cx + cw * (1 + ROAD_RUMBLE_WIDTH), rumble);
				DrawSpan(row, cx - cw, cx + cw, tarmac);

				// Lane markings live on the light segments only, which is
				// what leaves a gap between one dash and the next
				if (light) {
					for (int i = 1; i < ROAD_LANES; i++) {
						float lx = cx - cw + 2 * cw * i / ROAD_LANES;
						DrawSpan(row, lx - cw * ROAD_LANE_WIDTH, lx + cw * ROAD_LANE_WIDTH, lane);
					}
				}
			}
		}

		// The billboard stands on the near boundary of the segment it belongs to
		if (segment->sprite >= 0) {
			float sx = Edges[n].x + Edges[n].scale * segment->offset * ROAD_WIDTH * (RETRO_WIDTH / 2.0f);
			DrawSprite(dest, &Sprites[segment->sprite], sx, Edges[n].y, Edges[n].scale, shade);
		}
	}
}

//
// A section reaching curve over its first segments, holding it, and letting it
// go again, climbing height segment lengths as it does. The curve eases in and
// out so the road never breaks; the height eases across the whole section, so
// a hill leaves and arrives flat
//
void AddRoad(int enter, int hold, int leave, float curve, float height)
{
	int total = enter + hold + leave;
	float starty = RoadHeight;
	float endy = starty + height * ROAD_SEGMENT_LENGTH;

	for (int n = 0; n < total; n++) {
		if (RoadSegments >= ROAD_MAX_SEGMENTS) {
			RETRO_RageQuit("Too many road segments\n");
		}

		// How far into the run in, and into the run out, this segment sits
		float in = n < enter ? (float)n / enter : 1.0f;
		float out = n < enter + hold ? 0.0f : (float)(n - enter - hold) / leave;
		float ease = (1 - cos((float)n / total * M_PI)) / 2;

		Segment *segment = &Road[RoadSegments++];
		segment->curve = curve * in * in * (1 + cos(out * M_PI)) / 2;
		segment->y = starty + (endy - starty) * ease;
		segment->sprite = -1;
		segment->offset = 0;
	}

	RoadHeight = endy;
}

void DEMO_Initialize(void)
{
	// Init palette. One ramp per material, the haze the sky ends at up to the
	// material itself, so distance is a shade of the material's own hue
	for (int m = 0; m < ROAD_MATERIALS; m++) {
		RETRO_CreateGradientPalette(m * ROAD_SHADES, (m + 1) * ROAD_SHADES, ROAD_FOG, MaterialColor[m]);
		RETRO_SetColor((m + 1) * ROAD_SHADES - 1, MaterialColor[m]);
	}

	// The sky ends at the same haze, so the road runs out of contrast where
	// it meets it rather than stopping at a line
	RETRO_CreateGradientPalette(ROAD_SKY, ROAD_SKY + ROAD_SKY_SHADES / 2, ROAD_ZENITH, RETRO_GLAUCOUS);
	RETRO_CreateGradientPalette(ROAD_SKY + ROAD_SKY_SHADES / 2, RETRO_COLORS, RETRO_GLAUCOUS, ROAD_FOG);
	RETRO_SetColor(RETRO_COLORS - 1, ROAD_FOG);

	// Init track. The sections climb and descend the same heights, so the
	// road stands at zero where it loops, and the segments come to a multiple
	// of two rumble lengths, so the stripes alternate across the seam too
	AddRoad(40, 60, 40, 0, 0);
	AddRoad(40, 40, 40, ROAD_CURVE_MEDIUM, ROAD_HILL_LOW);
	AddRoad(40, 40, 40, 0, -ROAD_HILL_LOW);
	AddRoad(50, 50, 50, -ROAD_CURVE_HARD, 0);
	AddRoad(30, 60, 30, 0, ROAD_HILL_HIGH);
	AddRoad(40, 40, 40, ROAD_CURVE_EASY, -ROAD_HILL_HIGH);
	AddRoad(60, 60, 60, -ROAD_CURVE_EASY, ROAD_HILL_LOW);
	AddRoad(40, 50, 40, ROAD_CURVE_HARD, -ROAD_HILL_LOW);
	AddRoad(50, 80, 50, 0, 0);

	if (RoadSegments % (2 * ROAD_RUMBLE_LENGTH)) {
		RETRO_RageQuit("The track does not hold whole rumble stripes\n");
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
