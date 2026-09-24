//
// Rings
//
// Concentric solid washers on a shallow paraboloid,
//
//   z = BOWL · r²
//
// Ring i is a faceted band of rectangular cross-section: a top and a bottom
// RING_WIDTH across, and inner and outer walls BAND_DEPTH deep. Equal steps
// in r are the face-on bullseye, with the gaps the same width all the way
// out. The depth is what keeps a turned band thick. A sheet of no depth
// foreshortens to a line, and these are solids.
//
// A band is one hue, silver at the hole through magenta to blue at the rim.
// The light only picks a shade of that hue. Normals lean out around each
// band and are interpolated per pixel, so the highlight is round on a
// coarse ring and the lit end of the ramp is white.
// A tilted band shows the inner wall on its far side, facing the camera
// across the hole. Its normals lean in toward the hole, so it is lit from
// there. Turning the dish over brings the bottoms to the front.
// At the start each band is above the frame. The silver ring in the middle
// drops first and the rest follow outward, one at a time, straight down the
// screen. A band comes in laid down into a flat ellipse and slowly stands up
// face on as it settles, so the bullseye opens from the middle out.
// A band that has stood up never comes to rest. It leans, and the way it
// leans turns steadily round, while the lean itself swells from slight to
// nearly edge on and back. The big swings come and go on an axis that has
// moved on each time. Every band plays the same motion a little behind the
// band inside it, so each swing runs outward through the dish.
// The light swings round one side of the dish, cuts straight across the
// middle to the other side and swings round again.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define RING_COUNT 14
#define SEGMENTS 16 // quads around a band. At this radius 32 already lies
					// inside a pixel of a circle, and the bands are visibly sided

// Inner edge of the first band. The pitch is not stored: the last band's
// outer edge is 1, and RING_FILL of each step is the top of the washer.
// The rest of the step is the gap. The hole is 0.15 of the rim.
#define HOLE 0.15f
#define RING_FILL 0.58f

// Thin enough that an edge-on frame is a blade. The face-on band is the
// top; the wall is only a couple of pixels.
#define BAND_DEPTH 0.016f

// Almost flat. A deeper bowl stays thick when the nod lays the stack down.
#define BOWL 0.03f

// Shades of one band's hue, dark at a grazing angle up to the hue face on.
#define BAND_SHADES 16
#define SHADE_FLOOR 0.50f

// Pixels per model unit at z = 0. Radius 1 then falls just inside the
// screen edge, and a nearer rim overshoots it.
#define PROJECTION_SCALE 155.0f

// Each band starts DROP_DISTANCE above its place and takes DROP_TIME to ease
// onto it, RING_STAGGER after the band inside it. The distance hides even the
// rim above the frame: laid down, its far side is a metre behind the dish,
// where perspective pulls it down to 0.62 of its height.
#define DROP_DISTANCE 1.8f
#define DROP_TIME 3.0f
#define RING_STAGGER 0.30f

// The fall pose: a nod that lays a band down into a horizontal ellipse.
// RISE_OVERLAP before it lands, while the drop is easing off, it starts to
// stand up face on, over RISE_TIME. So it never hangs still between the two.
// On the way it swings RISE_TURN round the vertical and back, so a band half
// up is turned side on.
#define ENTER_NOD 1.55f
#define RISE_OVERLAP 0.5f
#define RISE_TIME 1.8f
#define RISE_TURN 0.8f

// The motion once a band has stood up. The direction it leans turns at
// TURN_SPEED, TURN_SWING faster at the low point of each swell and as much
// slower at the top, like a coin that spins quicker as it settles. That
// evens out the pace: the turn moves a band by the sine of its lean, so a
// band just off face on needs the quicker turn to keep up. The lean swells on a cosine between TILT_MIN and TILT_MAX, one
// swell every 2π / SWELL_SPEED s, the first from SWELL_START, once the
// bullseye is nearly complete. Until then the lean stays at TILT_MIN and
// keeps turning, so the bands do not stop while they wait. Each band runs
// FOLLOW_DELAY behind the band inside it. The direction never stops
// turning, so the band never stops, even at the top of a swell.
#define TURN_SPEED 0.7f
#define TURN_SWING 0.3f
#define TILT_MIN 0.4f
#define TILT_MAX 1.35f
#define SWELL_SPEED 1.5f
#define SWELL_START 9.0f
#define FOLLOW_DELAY 0.07f

// The light is a direction that runs round two circles of the same size at
// once, a quick one and a slow one the other way. Where the two cancel it
// points straight at the dish and every side is lit alike. That happens
// every 2π / (SPEED_1 - SPEED_2) s, and the lit side jumps across the
// middle. Between times it swings round at the mean of the two speeds.
// REACH is how far it leans off the axis at most; LIGHT_Z sets how dark the
// far side gets. Screen y grows downward, so the swing is counterclockwise.
#define LIGHT_REACH 1.0f
#define LIGHT_Z -0.55f
#define LIGHT_SPEED_1 3.44f
#define LIGHT_SPEED_2 -0.48f
#define LIGHT_PHASE_1 0.32f
#define LIGHT_PHASE_2 1.62f

// A small lean: the lit side rises toward white across a wide arc, and the
// far side keeps the band's colour. A hard tilt paints a white slice.
#define HIGHLIGHT_TILT 0.72f

// Pitch that puts the outer edge of the last band at radius 1.
static const float RING_PITCH = (1.0f - HOLE) / (RING_COUNT - 1 + RING_FILL);
static const float RING_WIDTH = RING_FILL * RING_PITCH;

struct RingStop {
	float t;
	unsigned char r, g, b;
};

// Silver at the hole, magenta through the middle, blue at the rim.
static const RingStop RingColors[] = {
	{ 0.00f, 228, 228, 234 },
	{ 0.12f, 186, 124, 180 },
	{ 0.34f, 140, 38, 142 },
	{ 0.58f, 64, 24, 138 },
	{ 0.82f, 16, 16, 108 },
	{ 1.00f, 8, 10, 46 },
};

static RETRO_Palette ColorAt(float t)
{
	const int stops = (int)(sizeof(RingColors) / sizeof(RingColors[0]));

	if (t <= RingColors[0].t) {
		return { RingColors[0].r, RingColors[0].g, RingColors[0].b };
	}
	for (int i = 1; i < stops; i++) {
		if (t > RingColors[i].t) {
			continue;
		}
		float span = RingColors[i].t - RingColors[i - 1].t;
		float k = span > 0.0f ? (t - RingColors[i - 1].t) / span : 0.0f;
		const RingStop &a = RingColors[i - 1];
		const RingStop &b = RingColors[i];
		return {
			(unsigned char)(a.r + (b.r - a.r) * k),
			(unsigned char)(a.g + (b.g - a.g) * k),
			(unsigned char)(a.b + (b.b - a.b) * k)
		};
	}
	const RingStop &last = RingColors[stops - 1];
	return { last.r, last.g, last.b };
}

// Four corners of one segment, in this order.
#define CORNER_INNER_TOP 0
#define CORNER_OUTER_TOP 1
#define CORNER_INNER_BOTTOM 2
#define CORNER_OUTER_BOTTOM 3

static int RingCorner(int base, int segment, int corner)
{
	return base + (segment % SEGMENTS) * 4 + corner;
}

// The inner wall faces the hole, so it cannot share the corners' outward
// normals. Each segment has two of its own, top and bottom, stored after
// all the corner normals, one band at a time.
#define CORNER_NORMALS (RING_COUNT * SEGMENTS * 4)
#define WALL_TOP 0
#define WALL_BOTTOM 1

static int WallNormal(int ring, int segment, int edge)
{
	return CORNER_NORMALS + (ring * SEGMENTS + segment % SEGMENTS) * 2 + edge;
}

static void BuildRings(Model3D *model)
{
	for (int ring = 0; ring < RING_COUNT; ring++) {
		float inner = HOLE + ring * RING_PITCH;
		float outer = inner + RING_WIDTH;
		float mid = (inner + outer) * 0.5f;
		// The top faces the camera. The body continues in +z, away from it.
		float ztop = BOWL * mid * mid;
		float zbottom = ztop + BAND_DEPTH;
		int base = model->vertices;
		int color = ring * BAND_SHADES;

		for (int s = 0; s < SEGMENTS; s++) {
			float angle = 2.0f * M_PI * s / SEGMENTS;
			float c = cosf(angle);
			float sn = sinf(angle);

			RETRO_AddModelVertex(model, inner * c, inner * sn, ztop);
			RETRO_AddModelVertex(model, outer * c, outer * sn, ztop);
			RETRO_AddModelVertex(model, inner * c, inner * sn, zbottom);
			RETRO_AddModelVertex(model, outer * c, outer * sn, zbottom);
		}

		for (int s = 0; s < SEGMENTS; s++) {
			int next = s + 1;
			int it0 = RingCorner(base, s, CORNER_INNER_TOP);
			int ot0 = RingCorner(base, s, CORNER_OUTER_TOP);
			int ib0 = RingCorner(base, s, CORNER_INNER_BOTTOM);
			int ob0 = RingCorner(base, s, CORNER_OUTER_BOTTOM);
			int it1 = RingCorner(base, next, CORNER_INNER_TOP);
			int ot1 = RingCorner(base, next, CORNER_OUTER_TOP);
			int ib1 = RingCorner(base, next, CORNER_INNER_BOTTOM);
			int ob1 = RingCorner(base, next, CORNER_OUTER_BOTTOM);

			// Outward windings: top toward -z, bottom toward +z, outer wall
			// away from the hole, inner wall toward it.
			RETRO_AddModelQuad(model, it0, it1, ot1, ot0, color);
			RETRO_AddModelQuad(model, ib0, ob0, ob1, ib1, color);
			RETRO_AddModelQuad(model, ot0, ot1, ob1, ob0, color);
			RETRO_AddModelQuad(model, it0, ib0, ib1, it1, color);
		}
	}
}

// One normal per corner, leaned out along that corner's radius. Shared by
// the two quads that meet there. Phong interpolates these across the facet
// and renormalises, so the highlight is not a blend of the corner colours.
// Down the outer wall the lean is all that is left: the wall's normal runs
// from the top's out to the bottom's, through the radius. The inner wall
// leans the other way, in toward the hole.
static void BuildHighlightNormals(Model3D *model)
{
	model->normals = CORNER_NORMALS + RING_COUNT * SEGMENTS * 2;
	int perring = SEGMENTS * 4;

	for (int ring = 0; ring < RING_COUNT; ring++) {
		int base = ring * perring;
		for (int s = 0; s < SEGMENTS; s++) {
			float ang = 2.0f * M_PI * s / SEGMENTS;
			float rx = cosf(ang) * HIGHLIGHT_TILT;
			float ry = sinf(ang) * HIGHLIGHT_TILT;
			int v = base + s * 4;

			model->normal[v + CORNER_INNER_TOP].dir = normalize(vec3{ rx, ry, -1.0f });
			model->normal[v + CORNER_OUTER_TOP].dir = normalize(vec3{ rx, ry, -1.0f });
			model->normal[v + CORNER_INNER_BOTTOM].dir = normalize(vec3{ rx, ry, 1.0f });
			model->normal[v + CORNER_OUTER_BOTTOM].dir = normalize(vec3{ rx, ry, 1.0f });
			model->normal[WallNormal(ring, s, WALL_TOP)].dir = normalize(vec3{ -rx, -ry, -1.0f });
			model->normal[WallNormal(ring, s, WALL_BOTTOM)].dir = normalize(vec3{ -rx, -ry, 1.0f });
		}
	}

	for (int f = 0; f < model->faces; f++) {
		Face *face = &model->face[f];
		for (int j = 0; j < face->vertices; j++) {
			face->vertexnormal[j] = face->vertex[j];
		}
	}

	// Every fourth quad of a band is its inner wall: it0, ib0, ib1, it1.
	for (int ring = 0; ring < RING_COUNT; ring++) {
		for (int s = 0; s < SEGMENTS; s++) {
			Face *face = &model->face[(ring * SEGMENTS + s) * 4 + 3];
			face->vertexnormal[0] = WallNormal(ring, s, WALL_TOP);
			face->vertexnormal[1] = WallNormal(ring, s, WALL_BOTTOM);
			face->vertexnormal[2] = WallNormal(ring, s + 1, WALL_BOTTOM);
			face->vertexnormal[3] = WallNormal(ring, s + 1, WALL_TOP);
		}
	}
}

// How far above its place this band still is. Zero once it has landed. Ring 0 is the middle and leaves at time 0; each band outward
// waits another RING_STAGGER. Half-cosine, so a band comes in off the top
// and settles.
static float RingFall(double time, int ring)
{
	double t = time - ring * (double)RING_STAGGER;

	if (t <= 0.0) {
		return DROP_DISTANCE;
	}
	if (t >= DROP_TIME) {
		return 0.0f;
	}

	float u = (float)(t / DROP_TIME);
	float k = 0.5f - 0.5f * cosf(u * (float)M_PI);
	return DROP_DISTANCE * (1.0f - k);
}

void DEMO_Render(double time, double deltatime)
{
	float angle1 = (float)fmod(time * LIGHT_SPEED_1 + LIGHT_PHASE_1, 2.0 * M_PI);
	float angle2 = (float)fmod(time * LIGHT_SPEED_2 + LIGHT_PHASE_2, 2.0 * M_PI);
	RETRO_Render.lightsource = RETRO_LightSource(
		LIGHT_REACH * 0.5f * (cosf(angle1) + cosf(angle2)),
		-LIGHT_REACH * 0.5f * (sinf(angle1) + sinf(angle2)),
		LIGHT_Z);

	// Vertices and faces are packed one band at a time: four corners and
	// four quads a segment. Each band is turned about the centre, then
	// raised up the screen by what is left of its fall. Screen y grows
	// downward, so the lift is subtracted.
	Model3D *model = RETRO_Get3DModel();
	int perring = SEGMENTS * 4;

	for (int ring = 0; ring < RING_COUNT; ring++) {
		double riseat = ring * (double)RING_STAGGER + DROP_TIME - RISE_OVERLAP;
		float rise = (float)smoothstep(riseat, riseat + RISE_TIME, time);
		float nod = ENTER_NOD * (1.0f - rise);
		float turn = RISE_TURN * sinf(rise * (float)M_PI);
		mat3 matrix = rotateY(turn) * rotateX(nod);

		// This band's own clock, FOLLOW_DELAY behind the band inside it. The
		// swell starts at its low point, and the lean grows in as the band
		// stands up. Turn the lean's direction onto x, tilt about it and turn
		// back.
		double follow = time - ring * (double)FOLLOW_DELAY;
		// Before the first swell the turn keeps the quicker pace of a low
		// point, which the swell then takes up at the same angle and speed.
		double phase = fmod((follow - SWELL_START) * SWELL_SPEED, 2.0 * M_PI);
		double swing = follow < SWELL_START ? TURN_SWING * (follow - SWELL_START) : TURN_SWING / SWELL_SPEED * sin(phase);
		float axis = (float)fmod(follow * TURN_SPEED + swing, 2.0 * M_PI);
		float swell = follow < SWELL_START ? 0.0f : 0.5f - 0.5f * (float)cos(phase);
		float tilt = (TILT_MIN + (TILT_MAX - TILT_MIN) * swell) * rise;
		matrix = rotateZ(axis) * rotateX(tilt) * rotateZ(-axis) * matrix;

		float fall = RingFall(time, ring);
		int base = ring * perring;
		for (int i = 0; i < perring; i++) {
			RETRO_RotateVertex(&model->vertex[base + i], matrix);
			model->vertex[base + i].rpos.y -= fall;
			model->normal[base + i].rdir = matrix * model->normal[base + i].dir;
		}
		for (int i = 0; i < SEGMENTS * 2; i++) {
			UnitVector *normal = &model->normal[WallNormal(ring, 0, WALL_TOP) + i];
			normal->rdir = matrix * normal->dir;
		}
	}

	RETRO_ProjectModel(PROJECTION_SCALE);
	RETRO_RenderModel(RETRO_POLY_PHONG);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
	for (int ring = 0; ring < RING_COUNT; ring++) {
		float t = ring / (float)(RING_COUNT - 1);
		RETRO_Palette bright = ColorAt(t);
		RETRO_Palette dark = {
			(unsigned char)(bright.r * SHADE_FLOOR),
			(unsigned char)(bright.g * SHADE_FLOOR),
			(unsigned char)(bright.b * SHADE_FLOOR)
		};
		int start = 1 + ring * BAND_SHADES;
		int body = start + BAND_SHADES * 3 / 4;
		RETRO_Palette hot = {
			(unsigned char)(bright.r + (255 - bright.r) * 0.45f),
			(unsigned char)(bright.g + (255 - bright.g) * 0.45f),
			(unsigned char)(bright.b + (255 - bright.b) * 0.45f)
		};
		RETRO_CreateGradientPalette(start, body, dark, bright);
		RETRO_CreateGradientPalette(body, start + BAND_SHADES, bright, hot);
	}

	Model3D *model = RETRO_Allocate3DModel();
	model->c = 1;
	model->shades = BAND_SHADES;
	BuildRings(model);
	RETRO_InitializeFaceNormals(model);
	BuildHighlightNormals(model);
}
