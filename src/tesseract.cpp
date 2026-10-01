//
// Tesseract
//
// A hypercube of beams and glass turning in four dimensions. The 16 corners
// are (±1, ±1, ±1, ±1). Two corners share an edge when they differ in one
// coordinate, which gives 32 edges, and four corners that differ in two
// span a square face, which gives 24. Each frame the corners turn in the
// XW plane, which has no 3D equivalent, and in YZ, the plane orthogonal to
// it - a double rotation - and then slowly in XZ, so the figure also
// tumbles. A turn in plane (p, q) is
//
//   p' = p cos a − q sin a
//   q' = p sin a + q cos a
//
// The 4D-to-3D step is a pinhole along w with the eye at W_EYE,
//
//   k = W_EYE / (W_EYE − w),   (x, y, z) ← k (x, y, z)
//
// so the cell nearest in w is drawn as the large outer cube and the
// farthest as the small inner one, and a turn through w walks each cell
// from one to the other, turning the figure inside out. A pinhole keeps
// lines straight and planes flat, so in 3D an edge is still a segment and a
// face still a flat quad. The 3D-to-2D step is the library's pinhole,
// written out here since the corners are not a model.
//
// An edge is a beam: a lit ball swept from one corner to the other,
// BEAM_BALLS to the pixel, each written at the depth of its own surface.
// What survives the depth test is the hull of the balls, a round bar with
// round ends, and it meets the other beams along the curves where the bars
// really cross. Its radius is BEAM_RADIUS k, so a beam thins as it recedes
// in w.
//
// A face is a pane of glass over whatever is already drawn behind it, set
// GLASS_BIAS beam radii back so the beams that frame it stay clear. One pane
// of opacity A takes a color C to C + A (GLASS − C), and n of them to
//
//   C + (1 − (1 − A)^n) (GLASS − C)
//
// which depends only on how many panes there are, not their order. So the
// palette is laid out as index = level BEAM_SHADES + shade: shade is the
// beam color underneath, 0 for none, and level the panes over it. Laying a
// pane is then adding to the level, a table lookup, and the faces need no
// sorting. A face is worth GLASS_PANES levels, and up to GLASS_GLINT more
// as it turns to mirror the light at the viewer,
//
//   glint = |N·H|^GLINT_POWER,   H = (L + V) / |L + V|
//
// Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropalette.h"
#include "lib/retropoly.h"

#define CORNERS 16
#define EDGES 32
#define FACES 24

#define W_SPEED 0.9f       // radians a second, in the XW plane
#define YZ_SPEED 0.6f      // radians a second, in the YZ plane
#define XZ_SPEED 0.25f     // radians a second, in the XZ plane
#define W_EYE 4.0f         // distance of the 4D eye along w
#define SCALE 44.0f        // pixels per unit after the 4D projection

#define BEAM_RADIUS 0.065f // of a beam, in the units of the corners
#define BEAM_SHADES 4      // palette entries a level spends: none, and the beam's three
#define BEAM_BALLS 3       // balls a pixel of its length, so each pixel finds one centered on it
#define BEAM_MAP 64        // the lit ball is drawn once at this size and scaled from it
#define BEAM_DARK RETRO_Palette{ 20, 60, 110 }
#define BEAM_LIGHT RETRO_Palette{ 130, 205, 255 }

#define GLASS_LEVELS (RETRO_COLORS / BEAM_SHADES)
#define GLASS_ALPHA 0.025f // opacity of one level
#define GLASS_PANES 2      // levels a face lays whichever way it is turned
#define GLASS_GLINT 12     // further levels on a face mirroring the light
#define GLASS_BIAS 1.5f    // beam radii a pane sits behind its corners, clear of its own beams
#define GLINT_POWER 24.0f
#define GLASS RETRO_Palette{ 196, 222, 240 }

struct Edge {
	int a, b;
};

struct Quad {
	int corner[4];
};

static vec4 Corner[CORNERS];
static Edge Edges[EDGES];
static Quad Faces[FACES];
static unsigned char BallMap[BEAM_MAP * BEAM_MAP];
static float BallDepth[BEAM_MAP * BEAM_MAP];
static unsigned char GlassTable[GLASS_PANES + GLASS_GLINT + 1][RETRO_COLORS];
static vec3 Light = { -0.4f, -0.4f, -0.82f }; // toward the light, in view space

//
// Turn the pair (p, q) by angle in their own plane
//
static void RotatePlane(float &p, float &q, float angle)
{
	float c = cos(angle), s = sin(angle);
	float np = p * c - q * s;
	q = p * s + q * c;
	p = np;
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float axw = fmod(time * W_SPEED, 2 * M_PI);
	float ayz = fmod(time * YZ_SPEED, 2 * M_PI);
	float axz = fmod(time * XZ_SPEED, 2 * M_PI);

	vec3 position[CORNERS];
	float radius[CORNERS];
	float focal = SCALE * RETRO_PROJECTION_EYEDISTANCE;

	for (int i = 0; i < CORNERS; i++) {
		vec4 p = Corner[i];
		RotatePlane(p.x, p.w, axw);
		RotatePlane(p.y, p.z, ayz);
		RotatePlane(p.x, p.z, axz);

		// 4D to 3D, along w
		float k = W_EYE / (W_EYE - p.w);
		position[i] = { p.x * k, p.y * k, p.z * k };
		radius[i] = BEAM_RADIUS * k;
	}

	RETRO_ClearDepthBuffer();

	// Beams. 3D to 2D, as RETRO_ProjectVertex
	for (Edge edge : Edges) {
		vec3 a = position[edge.a], b = position[edge.b];
		float qa = 1.0f / (SCALE * a.z + RETRO_PROJECTION_EYEDISTANCE);
		float qb = 1.0f / (SCALE * b.z + RETRO_PROJECTION_EYEDISTANCE);
		vec2 sa = { focal * a.x * qa, focal * a.y * qa };
		vec2 sb = { focal * b.x * qb, focal * b.y * qb };
		int steps = ceil(distance(sa, sb) * BEAM_BALLS) + 1;

		for (int step = 0; step <= steps; step++) {
			float t = (float)step / steps;
			vec3 r = mix(a, b, t);
			float ballradius = mix(radius[edge.a], radius[edge.b], t);
			float q = 1.0f / (SCALE * r.z + RETRO_PROJECTION_EYEDISTANCE);
			vec2 spos = { RETRO_WIDTH / 2.0f + focal * r.x * q, RETRO_HEIGHT / 2.0f + focal * r.y * q };
			RETRO_DrawDepthSprite(spos, q, 2 * ballradius * focal * q, SCALE * ballradius, BallMap, BallDepth, BEAM_MAP);
		}
	}

	// Glass, over the beams behind it
	vec3 halfway = normalize(Light + vec3{ 0.0f, 0.0f, -1.0f });
	for (Quad face : Faces) {
		PolygonPoint polygon[4] = {};
		for (int i = 0; i < 4; i++) {
			vec3 r = position[face.corner[i]];
			float q = 1.0f / (SCALE * r.z + RETRO_PROJECTION_EYEDISTANCE);
			polygon[i].pos = { RETRO_WIDTH / 2.0f + focal * r.x * q, RETRO_HEIGHT / 2.0f + focal * r.y * q };
			polygon[i].q = 1.0f / (SCALE * (r.z + GLASS_BIAS * radius[face.corner[i]]) + RETRO_PROJECTION_EYEDISTANCE);
		}

		vec3 n = normalize(cross(position[face.corner[1]] - position[face.corner[0]], position[face.corner[3]] - position[face.corner[0]]));
		float glint = pow(fabs(dot(n, halfway)), GLINT_POWER);
		int panes = GLASS_PANES + lround(GLASS_GLINT * glint);
		RETRO_DrawRemapPolygon(polygon, 4, GlassTable[panes]);
	}
}

void DEMO_Initialize(void)
{
	// Init palette: a beam ramp, and each of its entries under more and more glass
	for (int level = 0; level < GLASS_LEVELS; level++) {
		float cover = 1 - pow(1 - GLASS_ALPHA, level);
		for (int shade = 0; shade < BEAM_SHADES; shade++) {
			float k = (shade - 1.0f) / (BEAM_SHADES - 2);
			vec3 base = shade ? mix(vec3{ BEAM_DARK.r, BEAM_DARK.g, BEAM_DARK.b }, vec3{ BEAM_LIGHT.r, BEAM_LIGHT.g, BEAM_LIGHT.b }, k) : vec3{};
			vec3 color = mix(base, vec3{ GLASS.r, GLASS.g, GLASS.b }, cover);
			RETRO_SetColor(level * BEAM_SHADES + shade, color.x, color.y, color.z);
		}
	}

	// Laying panes adds to the level and leaves the shade
	for (int panes = 0; panes <= GLASS_PANES + GLASS_GLINT; panes++) {
		for (int i = 0; i < RETRO_COLORS; i++) {
			int level = MIN(i / BEAM_SHADES + panes, GLASS_LEVELS - 1);
			GlassTable[panes][i] = level * BEAM_SHADES + i % BEAM_SHADES;
		}
	}

	// The ball a beam is swept from, in shades 1 and up; 0 is its transparent outside
	RETRO_CreateBallMap(BallMap, BallDepth, BEAM_MAP, 1, BEAM_SHADES - 1);

	// Corner i has bit d of i as the sign of coordinate d
	for (int i = 0; i < CORNERS; i++) {
		Corner[i] = { i & 1 ? 1.0f : -1.0f, i & 2 ? 1.0f : -1.0f, i & 4 ? 1.0f : -1.0f, i & 8 ? 1.0f : -1.0f };
	}

	// Flipping one bit steps along one edge; count each edge from its lower end
	int edges = 0;
	for (int i = 0; i < CORNERS; i++) {
		for (int d = 0; d < 4; d++) {
			int j = i ^ (1 << d);
			if (i < j) {
				Edges[edges++] = { i, j };
			}
		}
	}

	// Flipping two bits in turn walks round one face; count each face from its lowest corner
	int faces = 0;
	for (int i = 0; i < CORNERS; i++) {
		for (int d = 0; d < 4; d++) {
			for (int e = d + 1; e < 4; e++) {
				int u = 1 << d, v = 1 << e;
				if (!(i & (u | v))) {
					Faces[faces++] = { { i, i | u, i | u | v, i | v } };
				}
			}
		}
	}
}
