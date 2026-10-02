//
// Polygon morph
//
// A flat shaded solid that turns from a cube into a sphere, a six-pointed
// star, a rounded cube, a saucer, a capsule, a cylinder, a barrel, an
// hourglass, a bucket, a twisted cube and a pinched cube, and back into
// the cube. Every shape is the same mesh: subcubequads.obj, a
// cube with a 7×7 grid of quads on each side. A vertex keeps its rest
// direction u = p / |p|, and the first three shapes are each a radius
// along it:
//
//   cube         the rest position itself
//   sphere       SPHERE_RADIUS
//   star         STAR_RADIUS (1 − STAR_SPIKE + STAR_SPIKE m^STAR_SHARPNESS),
//                m = max(|u_x|, |u_y|, |u_z|)
//
// The star's m is 1 on the six axes and 1/√3 on the diagonals, so its
// power lifts six spikes out of a smaller ball.
//
// The rounded cube is every point ROUNDED_RADIUS away from a smaller cube
// inside it. A vertex's nearest point on that core is its own position
// with each coordinate clamped to the core,
//
//   q = clamp(p, −(1 − ROUNDED_RADIUS), 1 − ROUNDED_RADIUS)
//   rounded cube = ROUNDED_SIZE (q + ROUNDED_RADIUS (p − q) / |p − q|)
//
// which leaves the middle of each side flat, bends the quads along an edge
// into a quarter cylinder and those at a corner into an eighth of a
// sphere. ROUNDED_RADIUS is a whole number of quads, so the flats end on
// a row of vertices.
//
// The capsule is the same about a line: every point CAPSULE_RADIUS away
// from a stretch of the y axis, which is the vertex's height clamped to
// ±CAPSULE_CORE. The rows of quads within the core become a wall and the
// rest close over it in two domes, and the core is then drawn out to
// CAPSULE_HEIGHT.
//
// The next five are turned about the y axis. A vertex lies on a square
// around that axis of half-side s = max(|x|, |z|), which is 1 on the
// cube's four sides and less on its top and bottom, and each shape puts
// the vertex at a radius r from the axis in its own direction, (x, z)
// becoming r (x, z) / |(x, z)|, and at a height:
//
//   saucer       r = SAUCER_RADIUS s
//                height SAUCER_RIM y ± SAUCER_DOME (1 − s²)
//   cylinder     r = CYLINDER_RADIUS s
//                height CYLINDER_HEIGHT y
//   barrel       the cylinder, r scaled by
//                BARREL_END + (1 − BARREL_END) (1 − y²)
//   hourglass    the cylinder, r scaled by
//                HOURGLASS_WAIST + (1 − HOURGLASS_WAIST) y²
//   bucket       the cylinder, r scaled by 1 + BUCKET_TAPER y
//
// r = s takes each square to the circle of the same size, so the cube's
// four sides become a wall and its top and bottom two discs, with the
// rims on the cube's own edges. The ± is the sign of y, and its term is
// zero on the wall, so the saucer's discs rise to a dome over a thin rim.
//
// The last two stay cubes:
//
//   twisted cube (x, z) turned about the y axis by TWIST_ANGLE y
//   pinched cube PINCHED_SIZE (1 − PINCHED_DEPTH d) p,
//                d = (1 − x²)(1 − y²) + (1 − y²)(1 − z²) + (1 − z²)(1 − x²)
//
// One coordinate of a vertex is ±1, which leaves d one term, the product
// over the other two: 1 in the middle of a side and 0 along its edges. So
// the sides sink toward the center and the edges stay straight.
//
// Because each vertex keeps its identity, a morph is the straight line
//
//   p(t) = (1 − t) p_from + t p_to
//
// with t eased by smoothstep, so a morph starts and stops without a jolt.
// A shape is held for TIME_HOLD seconds and a morph takes TIME_MORPH. The
// face normals are taken again every frame, because the rest normals
// describe the cube. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 0.9f // radians a second, about the middle axis
#define ROTATION_SPREAD 0.3f // the other two turn this much slower and faster

#define SPHERE_RADIUS 1.3f
#define STAR_RADIUS 1.8f // distance from the center to a spike's tip
#define STAR_SPIKE 0.55f // fraction of STAR_RADIUS the spikes stand out
#define STAR_SHARPNESS 6.0f // how narrow the spikes are
#define ROUNDED_SIZE 1.2f // distance from the center to a flat side
#define ROUNDED_RADIUS (4.0f / 7) // of the edges and corners, two of a side's seven quads
#define SAUCER_RADIUS 1.8f
#define SAUCER_RIM 0.1f // half the height of the rim
#define SAUCER_DOME 0.55f // height of a dome above the rim
#define CAPSULE_RADIUS 0.85f
#define CAPSULE_HEIGHT 0.9f // half the height of the wall, between the two domes
#define CAPSULE_CORE (3.0f / 7) // part of a side that becomes the wall, the middle three of its seven quads
#define CYLINDER_RADIUS 1.15f
#define CYLINDER_HEIGHT 1.25f // half of it, as a multiple of the cube's
#define BARREL_END 0.7f // fraction of the radius left at the top and bottom
#define HOURGLASS_WAIST 0.35f // fraction of the radius left at the middle
#define BUCKET_TAPER 0.35f // fraction of the radius gained at the top and lost at the bottom
#define TWIST_ANGLE (M_PI / 4) // turn at the top, and the opposite at the bottom
#define PINCHED_SIZE 1.2f // distance from the center to an edge, as a multiple of the cube's
#define PINCHED_DEPTH 0.5f // fraction of the way to the center the middle of a side sinks

#define TIME_HOLD 1.2 // seconds a shape is held
#define TIME_MORPH 1.6 // seconds a morph takes
#define TIME_SHAPE (TIME_HOLD + TIME_MORPH)

enum Shape {
	SHAPE_CUBE,
	SHAPE_SPHERE,
	SHAPE_STAR,
	SHAPE_ROUNDED,
	SHAPE_SAUCER,
	SHAPE_CAPSULE,
	SHAPE_CYLINDER,
	SHAPE_BARREL,
	SHAPE_HOURGLASS,
	SHAPE_BUCKET,
	SHAPE_TWIST,
	SHAPE_PINCHED,
	SHAPES
};

static vec3 ShapeVertex[SHAPES][RETRO_MAX_VERTICES];

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED * (1 - ROTATION_SPREAD), 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED * (1 + ROTATION_SPREAD), 2 * M_PI);

	// Calculate phase. Each shape is held, then morphs into the next
	double phase = fmod(time, SHAPES * TIME_SHAPE);
	int from = phase / TIME_SHAPE;
	int to = (from + 1) % SHAPES;
	float t = smoothstep(0.0, 1.0, (phase - from * TIME_SHAPE - TIME_HOLD) / TIME_MORPH);

	// Morph shapes
	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].pos = mix(ShapeVertex[from][i], ShapeVertex[to][i], t);
	}

	RETRO_InitializeFaceNormals(model);

	// Draw solid
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	// Init palette. Matte, because a flat lit face has one normal for all of
	// it and a specular highlight would flash the whole facet at once
	RETRO_CreateMattePalette(RETRO_GOLD);

	Model3D *model = RETRO_Load3DModel("assets/subcubequads.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;

	// Init shapes. The first three are a radius along the vertex's rest
	// direction, the rounded cube and the capsule stand off a smaller cube
	// and a line inside them, the next five are a radius from the y axis
	// and the last two stay cubes
	for (int i = 0; i < model->vertices; i++) {
		vec3 p = model->vertex[i].pos;
		vec3 u = normalize(p);
		float m = MAX(fabs(u.x), MAX(fabs(u.y), fabs(u.z)));

		float core = 1 - ROUNDED_RADIUS;
		vec3 q = clamp(p, -core, core);
		vec3 line = { 0, clamp(p.y, -CAPSULE_CORE, CAPSULE_CORE), 0 };

		// Radii from the y axis, each divided by the vertex's own so that
		// scaling (x, z) by it lands on that radius
		float square = MAX(fabs(p.x), fabs(p.z));
		float axis = hypotf(p.x, p.z);
		float saucer = SAUCER_RADIUS * square / axis;
		float cylinder = CYLINDER_RADIUS * square / axis;
		float barrel = cylinder * mix(BARREL_END, 1, 1 - p.y * p.y);
		float hourglass = cylinder * mix(HOURGLASS_WAIST, 1, p.y * p.y);
		float bucket = cylinder * (1 + BUCKET_TAPER * p.y);

		float dome = copysign(SAUCER_DOME * (1 - square * square), p.y);

		float twistcos = cos(p.y * TWIST_ANGLE);
		float twistsin = sin(p.y * TWIST_ANGLE);

		float x2 = 1 - p.x * p.x;
		float y2 = 1 - p.y * p.y;
		float z2 = 1 - p.z * p.z;
		float dent = x2 * y2 + y2 * z2 + z2 * x2;

		ShapeVertex[SHAPE_CUBE][i] = p;
		ShapeVertex[SHAPE_SPHERE][i] = u * SPHERE_RADIUS;
		ShapeVertex[SHAPE_STAR][i] = u * (STAR_RADIUS * (1 - STAR_SPIKE + STAR_SPIKE * (float)pow(m, STAR_SHARPNESS)));
		ShapeVertex[SHAPE_ROUNDED][i] = (q + normalize(p - q) * ROUNDED_RADIUS) * ROUNDED_SIZE;
		ShapeVertex[SHAPE_SAUCER][i] = { p.x * saucer, p.y * SAUCER_RIM + dome, p.z * saucer };
		ShapeVertex[SHAPE_CAPSULE][i] = line * (CAPSULE_HEIGHT / CAPSULE_CORE) + normalize(p - line) * CAPSULE_RADIUS;
		ShapeVertex[SHAPE_CYLINDER][i] = { p.x * cylinder, p.y * CYLINDER_HEIGHT, p.z * cylinder };
		ShapeVertex[SHAPE_BARREL][i] = { p.x * barrel, p.y * CYLINDER_HEIGHT, p.z * barrel };
		ShapeVertex[SHAPE_HOURGLASS][i] = { p.x * hourglass, p.y * CYLINDER_HEIGHT, p.z * hourglass };
		ShapeVertex[SHAPE_BUCKET][i] = { p.x * bucket, p.y * CYLINDER_HEIGHT, p.z * bucket };
		ShapeVertex[SHAPE_TWIST][i] = { p.x * twistcos - p.z * twistsin, p.y, p.x * twistsin + p.z * twistcos };
		ShapeVertex[SHAPE_PINCHED][i] = p * (PINCHED_SIZE * (1 - PINCHED_DEPTH * dent));
	}

	RETRO_InitializeLightSource(0, 0, -1);
}
