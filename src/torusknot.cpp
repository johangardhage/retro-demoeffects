//
// Torus knot
//
// A tube that follows a closed curve wound around a torus, KNOT_P times around
// its axis and KNOT_Q times around its tube before it meets itself again:
//
//   x = (TORUS_RADIUS + TORUS_TUBE cos(q φ)) cos(p φ)
//   y = (TORUS_RADIUS + TORUS_TUBE cos(q φ)) sin(p φ)
//   z = TORUS_TUBE sin(q φ)
//
// With p and q sharing no factor the curve is one loop, and a knot unless one
// of them is 1. 2 and 3 make the trefoil.
//
// The tube is a ring of TUBE_SIDES vertices set around the curve at each of
// TUBE_RINGS steps of φ, joined to the next ring by quads. A ring has to lie
// square to the curve, so it is laid out in two directions that are both at
// right angles to the tangent. The first is the outward normal of the torus,
//
//   n = (cos(q φ) cos(p φ), cos(q φ) sin(p φ), sin(q φ))
//
// which is at right angles to anything drawn on the torus, the curve included,
// and the second is the cross product of the tangent with it. Both come back to
// where they started when φ has gone all the way around, so the last ring joins
// the first with no twist to take up.
//
// A vertex of a ring points straight out from the curve, which is the normal of
// a round tube there, so the model carries those as its vertex normals and the
// shading is of the tube and not of its facets. The knot passes in front of
// itself, which neither the back-face test nor a sort of the faces can settle,
// so it is drawn into the depth buffer.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define KNOT_P 2 // turns around the axis of the torus
#define KNOT_Q 3 // turns around its tube
#define TORUS_RADIUS 1.0f // from the axis to the middle of the torus's tube
#define TORUS_TUBE 0.45f // radius of the torus's tube, which the curve lies on
#define TUBE_RADIUS 0.22f // radius of the tube drawn around the curve
#define TUBE_RINGS 100 // rings along the curve
#define TUBE_SIDES 10 // vertices around a ring
#define PROJECTION_SCALE 62.0f // pixels per model unit
#define ROTATION_SPEED_X 0.7 // radians a second
#define ROTATION_SPEED_Y 1.1
#define ROTATION_SPEED_Z 0.4

static int TubeVertex(int ring, int side)
{
	return (ring % TUBE_RINGS) * TUBE_SIDES + side % TUBE_SIDES;
}

static void BuildKnot(Model3D *model)
{
	for (int i = 0; i < TUBE_RINGS; i++) {
		float phi = 2 * M_PI * i / TUBE_RINGS;
		float cosp = cosf(KNOT_P * phi);
		float sinp = sinf(KNOT_P * phi);
		float cosq = cosf(KNOT_Q * phi);
		float sinq = sinf(KNOT_Q * phi);
		float reach = TORUS_RADIUS + TORUS_TUBE * cosq;

		// The curve, its derivative by φ, and the normal of the torus it lies on
		vec3 center = { reach * cosp, reach * sinp, TORUS_TUBE * sinq };
		vec3 tangent = normalize(vec3{
			-KNOT_Q * TORUS_TUBE * sinq * cosp - KNOT_P * reach * sinp,
			-KNOT_Q * TORUS_TUBE * sinq * sinp + KNOT_P * reach * cosp,
			KNOT_Q * TORUS_TUBE * cosq });
		vec3 normal = { cosq * cosp, cosq * sinp, sinq };
		vec3 binormal = cross(tangent, normal);

		for (int j = 0; j < TUBE_SIDES; j++) {
			float theta = 2 * M_PI * j / TUBE_SIDES;
			vec3 out = normal * cosf(theta) + binormal * sinf(theta);
			vec3 pos = center + out * TUBE_RADIUS;

			int vertex = RETRO_AddModelVertex(model, pos.x, pos.y, pos.z);
			model->normal[vertex].dir = out;
		}
	}
	model->normals = model->vertices;

	// Join each ring to the next, wound so the faces look out of the tube
	for (int i = 0; i < TUBE_RINGS; i++) {
		for (int j = 0; j < TUBE_SIDES; j++) {
			RETRO_AddModelQuad(model, TubeVertex(i, j), TubeVertex(i, j + 1), TubeVertex(i + 1, j + 1), TubeVertex(i + 1, j));
		}
	}

	for (int i = 0; i < model->faces; i++) {
		for (int j = 0; j < model->face[i].vertices; j++) {
			model->face[i].vertexnormal[j] = model->face[i].vertex[j];
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	// Calculate rotation
	float ax = fmod(time.total * ROTATION_SPEED_X, 2 * M_PI);
	float ay = fmod(time.total * ROTATION_SPEED_Y, 2 * M_PI);
	float az = fmod(time.total * ROTATION_SPEED_Z, 2 * M_PI);

	// Draw knot
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(PROJECTION_SCALE);
	RETRO_RenderModel(RETRO_POLY_PHONG);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreatePlasticPalette(RETRO_MARIGOLD, 30);

	Model3D *model = RETRO_Allocate3DModel();
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;
	BuildKnot(model);
}
