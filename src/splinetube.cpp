//
// Spline tube
//
// A tube that follows a closed spline whose control points are on the move.
// The control points stand evenly around a ring, and each rides two waves that
// run around it, one moving it in and out from the axis and one along it:
//
//   radius = RING_RADIUS + RADIAL_SWING sin(radialphase + RADIAL_LOBES angle)
//   z      = AXIAL_SWING sin(axialphase + AXIAL_LOBES angle)
//
// The two waves run at their own speeds, so the loop never settles into one
// shape turning in place.
//
// The curve through the control points is a Catmull-Rom spline. Between control
// points P1 and P2, with P0 before them and P3 after, it is the cubic
//
//   C(t) = ½ (2 P1 + (P2 − P0) t + (2 P0 − 5 P1 + 4 P2 − P3) t²
//             + (3 P1 − P0 − 3 P2 + P3) t³)
//
// which is at P1 for t = 0 and at P2 for t = 1, heading along P2 − P0 as it
// leaves and along P3 − P1 as it arrives. The next piece leaves the way this
// one arrived, so the curve has no corner at a control point.
//
// The tube is a ring of TUBE_SIDES vertices set around the curve at each of
// RING_STEPS steps of every piece, joined to the next ring by quads. A ring is
// laid out in two directions at right angles to the tangent, and since the
// curve is a new one every frame there is no surface to take them from. So the
// first ring picks a direction, and each ring after it takes over the direction
// of the ring before with the part along its own tangent removed. That carries
// the direction along the curve without turning it about the curve, so the
// quads between two rings are not wrung.
//
// Carried all the way around, the direction does not come back to the one it
// started as, but to that turned about the tangent by some angle. Each ring is
// turned back by its share of that angle, in proportion to how far along the
// curve it is, so the last ring meets the first.
//
// A vertex of a ring points straight out from the curve, which is the normal of
// a round tube there, so those are written as the vertex normals along with the
// vertices. The squares are two colors, each with a ramp of its own.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define CONTROL_POINTS 16
#define RING_RADIUS 1.3f // from the axis to a control point at rest
#define RADIAL_SWING 0.3f // how far a control point moves in and out
#define RADIAL_LOBES 3 // waves around the ring
#define RADIAL_SPEED 1.3 // radians a second
#define AXIAL_SWING 0.5f // how far a control point moves along the axis
#define AXIAL_LOBES 2
#define AXIAL_SPEED -0.9
#define TUBE_RADIUS 0.2f
#define RING_STEPS 6 // rings between two control points
#define TUBE_RINGS (CONTROL_POINTS * RING_STEPS)
#define TUBE_SIDES 8 // vertices around a ring
#define CHECK_RINGS 3 // rings a square is long
#define CHECK_SIDES 2 // and sides it is wide
#define TUBE_SHADES ((RETRO_COLORS - 1) / 2) // palette entries a color's ramp covers
#define PROJECTION_SCALE 52.0f // pixels per model unit
#define ROTATION_SPEED_X 0.5 // radians a second
#define ROTATION_SPEED_Y 0.8
#define ROTATION_SPEED_Z 0.3

static int TubeVertex(int ring, int side)
{
	return (ring % TUBE_RINGS) * TUBE_SIDES + side % TUBE_SIDES;
}

//
// The spline at t along the piece that leaves control point piece, and the
// derivative there
//
static vec3 SplinePoint(const vec3 *control, int piece, float t, vec3 *derivative)
{
	vec3 p0 = control[(piece + CONTROL_POINTS - 1) % CONTROL_POINTS];
	vec3 p1 = control[piece];
	vec3 p2 = control[(piece + 1) % CONTROL_POINTS];
	vec3 p3 = control[(piece + 2) % CONTROL_POINTS];

	*derivative = RETRO_CatmullRomDerivative(p0, p1, p2, p3, t);

	return RETRO_CatmullRom(p0, p1, p2, p3, t);
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED_X, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED_Y, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED_Z, 2 * M_PI);

	// Calculate phase, of the two waves the control points ride
	float radialphase = fmod(time * RADIAL_SPEED, 2 * M_PI);
	float axialphase = fmod(time * AXIAL_SPEED, 2 * M_PI);

	// Move the control points
	vec3 control[CONTROL_POINTS];
	for (int i = 0; i < CONTROL_POINTS; i++) {
		float angle = 2 * M_PI * i / CONTROL_POINTS;
		float radius = RING_RADIUS + RADIAL_SWING * sinf(radialphase + RADIAL_LOBES * angle);

		control[i] = { radius * cosf(angle), radius * sinf(angle), AXIAL_SWING * sinf(axialphase + AXIAL_LOBES * angle) };
	}

	// Carry a direction around the curve. It starts as the axis, which the curve
	// goes around and never runs along
	vec3 center[TUBE_RINGS];
	vec3 tangent[TUBE_RINGS];
	vec3 normal[TUBE_RINGS];
	vec3 carried = { 0, 0, 1 };
	for (int i = 0; i < TUBE_RINGS; i++) {
		vec3 derivative;
		center[i] = SplinePoint(control, i / RING_STEPS, (float)(i % RING_STEPS) / RING_STEPS, &derivative);
		tangent[i] = normalize(derivative);
		carried = normalize(carried - tangent[i] * dot(carried, tangent[i]));
		normal[i] = carried;
	}

	// The angle it comes back to the first ring turned by
	carried = normalize(carried - tangent[0] * dot(carried, tangent[0]));
	float twist = atan2f(dot(carried, cross(tangent[0], normal[0])), dot(carried, normal[0]));

	// Sweep the tube
	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < TUBE_RINGS; i++) {
		vec3 binormal = cross(tangent[i], normal[i]);

		for (int j = 0; j < TUBE_SIDES; j++) {
			float theta = 2 * M_PI * j / TUBE_SIDES - twist * i / TUBE_RINGS;
			vec3 out = normal[i] * cosf(theta) + binormal * sinf(theta);

			model->vertex[TubeVertex(i, j)].pos = center[i] + out * TUBE_RADIUS;
			model->normal[TubeVertex(i, j)].dir = out;
		}
	}

	// Draw tube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(PROJECTION_SCALE);
	RETRO_RenderModel(RETRO_POLY_PHONG);
}

void DEMO_Initialize(void)
{
	// Init palette, one ramp per color of the squares. Entry 0 stays black
	RETRO_Palette palette[RETRO_COLORS] = {};
	RETRO_CreatePhongRamp(&palette[1], TUBE_SHADES, RETRO_AZURE, RETRO_K_SPECULAR, 30, 255);
	RETRO_CreatePhongRamp(&palette[1 + TUBE_SHADES], TUBE_SHADES, RETRO_WHITE, RETRO_K_SPECULAR, 30, 255);
	RETRO_SetPalette(palette);

	Model3D *model = RETRO_Allocate3DModel();
	model->c = 1;
	model->shades = TUBE_SHADES;
	model->vertices = TUBE_RINGS * TUBE_SIDES;
	model->normals = model->vertices;

	// Join each ring to the next, wound so the faces look out of the tube, and
	// color them in squares
	for (int i = 0; i < TUBE_RINGS; i++) {
		for (int j = 0; j < TUBE_SIDES; j++) {
			RETRO_AddModelQuad(model, TubeVertex(i, j), TubeVertex(i, j + 1), TubeVertex(i + 1, j + 1), TubeVertex(i + 1, j), (i / CHECK_RINGS + j / CHECK_SIDES) % 2 * TUBE_SHADES);
		}
	}

	for (int i = 0; i < model->faces; i++) {
		for (int j = 0; j < model->face[i].vertices; j++) {
			model->face[i].vertexnormal[j] = model->face[i].vertex[j];
		}
	}

	RETRO_InitializeLightSource(0, 0, -1);
}
