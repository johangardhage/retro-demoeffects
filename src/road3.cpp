//
// Pseudo 3D road 3
//
// The same road as Road, projected as quads instead of walked as scanlines.
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
// anything it hides projects inside that band. Nothing is drawn above the
// road, so what is left there is the cleared screen.
//
// Rumble strips, tarmac and grass alternate between two colors every
// ROAD_RUMBLE_LENGTH segments; the count of segments is a multiple of that
// period, so the stripes still alternate across the seam where the track
// loops. Lane markings are drawn on the light segments only, which is what
// makes them dashes rather than lines.
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

#define ROAD_CAMERA_DEPTH (1.0 / tan((ROAD_FIELD_OF_VIEW / 2.0) * DEG2RAD))

// Curve is an acceleration, in world units of x per segment squared, and a
// hill is given in segment lengths of climb over the section it belongs to
#define ROAD_CURVE_EASY 2.0
#define ROAD_CURVE_MEDIUM 4.0
#define ROAD_CURVE_HARD 6.0
#define ROAD_HILL_LOW 20.0
#define ROAD_HILL_HIGH 45.0

// One entry per surface, and 0 for the screen the road is drawn on
enum { COLOR_GRASS_DARK = 1, COLOR_GRASS_LIGHT, COLOR_RUMBLE_DARK, COLOR_RUMBLE_LIGHT,
	COLOR_TARMAC_DARK, COLOR_TARMAC_LIGHT, COLOR_LANE };

struct Segment {
	float curve; // world units of x the centre gains per segment, per segment
	float y; // the height the road stands at where the segment begins
};

// A projected segment boundary: the centre of the road, the scanline it falls
// on, and its half-width in pixels
struct Edge {
	float x;
	float y;
	float w;
};

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

void DEMO_Render(double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();

	// Drive. The track loops, so the distance travelled wraps at its length
	double tracklength = RoadSegments * (double)ROAD_SEGMENT_LENGTH;
	static double position = 0;
	position = fmod(position + deltatime * ROAD_SPEED, tracklength);

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

		x += dx;
		dx += segment->curve;
	}

	// Draw the quads back to front, so a near one paints over whatever it hides
	for (int n = ROAD_DRAW_DISTANCE - 1; n >= 0; n--) {
		int index = (base + n) % RoadSegments;
		int light = (index / ROAD_RUMBLE_LENGTH) & 1;

		unsigned char grass = light ? COLOR_GRASS_LIGHT : COLOR_GRASS_DARK;
		unsigned char rumble = light ? COLOR_RUMBLE_LIGHT : COLOR_RUMBLE_DARK;
		unsigned char tarmac = light ? COLOR_TARMAC_LIGHT : COLOR_TARMAC_DARK;

		// The quad spans the scanlines between its far and its near boundary.
		// Both are still projected when the near one is behind the camera,
		// where it lands far below the screen and only the clipping is left
		float ytop = Edges[n + 1].y;
		float ybottom = Edges[n].y;

		if (ybottom <= ytop || ybottom <= 0 || ytop >= RETRO_HEIGHT) {
			continue;
		}

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

			// Lane markings live on the light segments only, which is what
			// leaves a gap between one dash and the next
			if (light) {
				for (int i = 1; i < ROAD_LANES; i++) {
					float lx = cx - cw + 2 * cw * i / ROAD_LANES;
					DrawSpan(row, lx - cw * ROAD_LANE_WIDTH, lx + cw * ROAD_LANE_WIDTH, COLOR_LANE);
				}
			}
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
	}

	RoadHeight = endy;
}

void DEMO_Initialize(void)
{
	// Init palette. One flat color per surface, and black for the screen the
	// road is drawn on, which is all that is left above the horizon
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(COLOR_GRASS_DARK, RETRO_HUNTERGREEN);
	RETRO_SetColor(COLOR_GRASS_LIGHT, RETRO_FORESTGREEN);
	RETRO_SetColor(COLOR_RUMBLE_DARK, RETRO_FIREBRICK);
	RETRO_SetColor(COLOR_RUMBLE_LIGHT, RETRO_LIGHTGRAY);
	RETRO_SetColor(COLOR_TARMAC_DARK, RETRO_CHARCOAL);
	RETRO_SetColor(COLOR_TARMAC_LIGHT, RETRO_DIMGRAY);
	RETRO_SetColor(COLOR_LANE, RETRO_LIGHTGRAY);

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
}
