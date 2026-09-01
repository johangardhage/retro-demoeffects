//
// Pseudo 3D road
//
// The same road as Road 3, walked as scanlines the way the arcade machines
// drew it instead of projected as quads: one row at a time, bottom to top,
// with no per-pixel and no per-quad divide anywhere in the frame. Every
// scanline below the horizon is a distance, and those distances are a table
// built once. Inverting the projection of the ground plane, a camera at
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
// spans, the road costs a screen, whatever the track does. Nothing is drawn
// above it, so what is left there is the cleared screen, and the row the walk
// stops on is the road running out of table rather than a horizon.
//
// Stripes come from the distance too: the tarmac, the rumble and the grass
// switch between two colors every ROAD_STRIPE_LENGTH of world z, so they flow
// through the perspective on their own, which a walk over quads cannot do
// without banding them per quad. The track holds a whole number of stripe
// pairs, so they still alternate where it loops.
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

// One entry per surface, and 0 for the screen the road is drawn on
enum { COLOR_GRASS_DARK = 1, COLOR_GRASS_LIGHT, COLOR_RUMBLE_DARK, COLOR_RUMBLE_LIGHT,
	COLOR_TARMAC_DARK, COLOR_TARMAC_LIGHT, COLOR_LANE };

struct Segment {
	float curve; // pixels across the road centre gains per scanline, per scanline
	float hill; // table entries a scanline takes on top of the one the flat road takes
};

Segment Road[ROAD_MAX_SEGMENTS];
int RoadSegments = 0;

float ZMap[ROAD_ZMAP_SIZE]; // the distance the row looks at, nearest first
float WidthMap[ROAD_ZMAP_SIZE]; // half the road, in pixels, at that distance
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

	for (int y = RETRO_HEIGHT - 1; y >= ROAD_HORIZON; y--) {
		int entry = (int)zi;
		if (entry >= ZMapEntries) {
			break; // the table has run past the draw distance, and the road with it
		}

		float z = ZMap[entry];
		float w = WidthMap[entry];

		// The stripes belong to the world, not to the rows, so they are read
		// off the distance the row is looking at
		int light = (int)((position + z) / ROAD_STRIPE_LENGTH) & 1;
		unsigned char grass = light ? COLOR_GRASS_LIGHT : COLOR_GRASS_DARK;
		unsigned char rumble = light ? COLOR_RUMBLE_LIGHT : COLOR_RUMBLE_DARK;
		unsigned char tarmac = light ? COLOR_TARMAC_LIGHT : COLOR_TARMAC_DARK;

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
				DrawSpan(row, lx - w * ROAD_LANE_WIDTH, lx + w * ROAD_LANE_WIDTH, COLOR_LANE);
			}
		}

		// The segment this row is looking at accelerates the centre and sets
		// how much of the table the next row takes
		Segment *segment = &Road[(int)((position + z) / ROAD_SEGMENT_LENGTH) % RoadSegments];

		dx += segment->curve;
		x += dx;
		zi += MAX(1 + segment->hill, (float)ROAD_MIN_STEP);
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
	}
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

	// Init the tables, nearest row first. Row H/2 + i looks at the ground
	// z = D h (H/2) / i, and the half row keeps the horizon itself out of the
	// table, where i is zero and the distance is not a number. Entries past
	// the draw distance are built but never walked into
	for (int i = 0; i < ROAD_ZMAP_SIZE; i++) {
		float rows = ROAD_ZMAP_SIZE - i - 0.5f; // rows below the horizon, nearest first
		float z = ROAD_CAMERA_DEPTH * ROAD_CAMERA_HEIGHT * ROAD_HORIZON / rows;

		ZMap[i] = z;
		WidthMap[i] = ROAD_CAMERA_DEPTH * ROAD_WIDTH * (RETRO_WIDTH / 2.0f) / z;

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
}
