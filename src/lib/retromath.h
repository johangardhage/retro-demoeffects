//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMATH_H_
#define _RETROMATH_H_

#include "retro.h"
#include "retromodel.h"

//
// Single vertex and unit vector rotation, without a model
//

// Same R as RETRO_RotateVertices, applied to one vertex that is not
// necessarily part of model->vertex - the ring and cloud effects that carry
// their own loose points instead of a loaded mesh.
inline void RETRO_RotateVertex(Vertex *vertex, const mat3 &matrix)
{
	vertex->rpos = matrix * vertex->pos;
}

inline void RETRO_RotateUnitVector(UnitVector *direction, const mat3 &matrix)
{
	direction->rdir = matrix * direction->dir;
}

//
// Model rotation, translation and projection
//

inline void RETRO_RotateVertices(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].rpos = model->matrix * model->vertex[i].pos;
	}
}

inline void RETRO_RotateVertexNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->normals; i++) {
		model->normal[i].rdir = model->matrix * model->normal[i].dir;
	}
}

// The face's frame - its normal and the tangent and bitangent built on it.
// All three turn together: T and B are perpendicular to N by construction and
// only stay so in view space if one matrix carries all of them. N goes to the
// flat and env shading; T and B reach the drawers as a TangentFrame, and have
// to turn with the model or the relief stays keyed to the screen.
inline void RETRO_RotateFaceFrames(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		model->face[i].tangent.rdir = model->matrix * model->face[i].tangent.dir;
		model->face[i].bitangent.rdir = model->matrix * model->face[i].bitangent.dir;
		model->face[i].facenormal.rdir = model->matrix * model->face[i].facenormal.dir;
	}
}

inline void RETRO_RotateModel(const mat3 &matrix, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	model->matrix = matrix;
	RETRO_RotateVertices(model);
	RETRO_RotateVertexNormals(model);
	RETRO_RotateFaceFrames(model);
}

inline void RETRO_RotateModel(float ax, float ay, float az, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	model->matrix = rotate(ax, ay, az);
	RETRO_RotateVertices(model);
	RETRO_RotateVertexNormals(model);
	RETRO_RotateFaceFrames(model);
}

// p' = R p + t, the translation half of a model's placement. It goes on the
// rotated coordinates, so the model turns about its own centre and is then
// carried to where it stands. Added to the model's own vertices instead it
// would be rotated too, and the model would swing around the origin.
//
// RETRO_ProjectModel's screen centre cannot stand in for this. That offset is
// in pixels, applied after the divide, so it neither shrinks with distance nor
// moves the model in z at all.
inline void RETRO_TranslateModel(float tx, float ty, float tz, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].rpos += vec3{ tx, ty, tz };
	}
}

// Pinhole projection. Depth is scale*rz + eyedistance, and the screen point is
//
//   q  = 1 / depth
//   sx = cx + scale * rx * eyedistance * q
//   sy = cy + scale * ry * eyedistance * q
//
// RETRO_ProjectVertex is this, and RETRO_ProjectModel is it over every vertex
// of a model. The principal point moves with cx, cy. A demo whose camera does
// not fit the rest - one that wants a focal length per axis, or a depth it
// works out for itself - writes the three lines above with what it has.
#define RETRO_PROJECTION_EYEDISTANCE 250

// Pixels per model unit when the model does not say. A model built in pixels
// passes 1; one built at unit size passes the size it wants to appear.
#define RETRO_PROJECTION_SCALE 50

//
// Screen point of a vertex whose rotated coordinates are already the ones the
// camera sees: rx across the screen, ry down it, rz along the view.
//
//   depth = scale * rz + eyedistance
//   q     = 1 / depth
//   sx    = cx + scale * eyedistance * rx * q
//   sy    = cy + scale * eyedistance * ry * q
//
// The eye's distance from the screen is in pixels, so scale * eyedistance is
// the focal length and is what sets the field of view. The principal point is
// taken as floats, so a horizon need not land on a whole one.
//
// A vertex at or behind the near plane is given q = 0 and parked at the
// principal point, for the caller to drop.
//
inline void RETRO_ProjectVertex(Vertex *vertex, float scale = RETRO_PROJECTION_SCALE, float cx = (RETRO_WIDTH / 2.0), float cy = (RETRO_HEIGHT / 2.0), float eyedistance = RETRO_PROJECTION_EYEDISTANCE)
{
	float depth = scale * vertex->rpos.z + eyedistance;

	if (depth <= 1.0f) {
		vertex->q = 0.0f;
		vertex->spos = { cx, cy };
	} else {
		float focal = scale * eyedistance;

		vertex->q = 1.0f / depth;
		vertex->spos = { cx + focal * vertex->rpos.x * vertex->q, cy + focal * vertex->rpos.y * vertex->q };
	}
}

inline void RETRO_ProjectModel(float scale = RETRO_PROJECTION_SCALE, float cx = (RETRO_WIDTH / 2.0), float cy = (RETRO_HEIGHT / 2.0), Model3D *model = NULL, float eyedistance = RETRO_PROJECTION_EYEDISTANCE)
{
	model = model ? model : RETRO_Get3DModel();

	// The model stands at the origin with the eye that far in front of it, and
	// its origin lands on the principal point. Moving that shifts every projected
	// vertex by the same pixels, so a demo can carry the model about the screen
	// with cx, cy alone. What it cannot do that way is move it in depth, which
	// is RETRO_TranslateModel's job
	for (int i = 0; i < model->vertices; i++) {
		RETRO_ProjectVertex(&model->vertex[i], scale, cx, cy, eyedistance);
	}
	model->eye = eyedistance / scale;
}

//
// Unit vectors and vertices, as written
//
// UnitVector::dir is the vector as authored. rdir is that vector after
// a rotation (RETRO_RotateUnitVector, RETRO_ViewUnitVector). RETRO_RotatedDot
// reads the rotated slot, because the shaded renderers have already turned
// the model. The helpers below read and write the authored slot, which is
// what a path tangent or a cross of two axes needs. RETRO_LightSource is
// the exception: a light has no orientation of its own to rotate out of,
// so it fills both slots directly.
//
// A UnitVector is unit as it is written. Fill dir, then wrap it in
// RETRO_NormalizeUnitVector. A zero vector (the cross of two parallel
// inputs) is stored as zero; the only reader that has to care is the
// next cross, which then also yields zero.
//
inline UnitVector RETRO_NormalizeUnitVector(UnitVector direction)
{
	direction.dir = normalize(direction.dir);
	return direction;
}

// A light source direction: normalized and valid for RETRO_RotatedDot
// immediately, because a light has no orientation of its own to rotate
// out of - it is given directly in whatever space shading happens in,
// so both slots are filled here rather than left for a rotate or view
// call that would never come.
inline UnitVector RETRO_LightSource(float x, float y, float z)
{
	vec3 dir = normalize(vec3{ x, y, z });
	return { dir, dir };
}

// D1 × D2 on the authored components, then normalized: the magnitude
// (|D1||D2|sin theta) is thrown away, because a UnitVector is unit as
// written, and this is how one is built out of two others. Parallel
// inputs write the zero direction.
inline UnitVector RETRO_UnitCrossProduct(UnitVector d1, UnitVector d2)
{
	return { normalize(cross(d1.dir, d2.dir)) };
}

// s · D. D is unit, so s is a length. The result is a Vertex because it is
// a displacement, not a unit direction, and not a position until
// RETRO_AddVertex lands it on one.
inline Vertex RETRO_ScaleUnitVector(UnitVector direction, float s)
{
	return { direction.dir * s };
}

// Scaling a Vertex is the same product, on something that is already a
// displacement (or a point from the origin).
inline Vertex RETRO_ScaleVertex(Vertex vertex, float s)
{
	return { vertex.pos * s };
}

inline Vertex RETRO_AddVertex(Vertex a, Vertex b)
{
	return { a.pos + b.pos };
}

// D1 · D2, taken on the rotated directions. Both are unit, so this is already
// the cosine of the angle between them and there is nothing to divide out.
inline float RETRO_RotatedDot(UnitVector d1, UnitVector d2)
{
	return dot(d1.rdir, d2.rdir);
}

//
// Face visibility and index sort
//

// Farther face first: greater mean depth belongs before lesser, which is
// the painter's order RETRO_SortFaces asks for.
inline bool RETRO_FaceDepthBefore(int a, int b, Model3D *model)
{
	return model->face[a].depth > model->face[b].depth;
}

// Lower model-space x, then y, then z. Groups split-mesh copies of one
// vertex so they sit adjacent in RETRO_RenderDotModel.
inline bool RETRO_VertexPosBefore(int a, int b, Model3D *model)
{
	vec3 pa = model->vertex[a].pos, pb = model->vertex[b].pos;
	if (pa.x != pb.x) return pa.x < pb.x;
	if (pa.y != pb.y) return pa.y < pb.y;
	return pa.z < pb.z;
}

// In-place quicksort of order[lo..hi]. Pivot is the middle element.
// before(a, b, model) is the order: true means a belongs before b.
// RETRO_SortFaces passes drawface and RETRO_FaceDepthBefore;
// RETRO_RenderDotModel passes a vertex index list and RETRO_VertexPosBefore.
inline void RETRO_QuickSort(int *order, int lo, int hi, bool (*before)(int a, int b, Model3D *model), Model3D *model)
{
	int i = lo;
	int j = hi;
	int pivot = order[(lo + hi) / 2];

	while (i <= j) {
		while (before(order[i], pivot, model)) {
			i++;
		}
		while (before(pivot, order[j], model)) {
			j--;
		}

		if (i <= j) {
			SWAP(order[i], order[j]);
			i++;
			j--;
		}
	}

	if (i < hi) {
		RETRO_QuickSort(order, i, hi, before, model);
	}
	if (lo < j) {
		RETRO_QuickSort(order, lo, j, before, model);
	}
}

// Drop faces behind the near plane (any vertex with q <= 0). Front-facing is
// the screen-space cross product (s1 - s0) × (s2 - s0). The projection keeps
// view x and y, and the frame is y down with +z away from the viewer, so this
// product carries the sign of the face normal's z: it is negative exactly when
// the outward normal (the right-hand rule on the same winding that
// RETRO_InitializeFaceNormals uses) points back at the viewer.
// Painter's algorithm: the survivors go into drawface sorted by mean depth, far
// to near, which is the list the renderers draw. With backfaces a face goes in
// whether it faces the viewer or not, and its Face::frontfacing says which side
// is showing, so a renderer can draw both: the Glenz and wireframe paths by
// choosing a palette contribution per side, the shaded ones by reversing the
// normal of the side that is turned away.
//
// A face dropped at the near plane never reaches the winding test, so it never
// reaches drawface either and its frontfacing is left as it stands. There is no
// answer to give: its corners were parked on the principal point with q = 0, so
// the cross product would be meaningless.
inline void RETRO_SortFaces(bool backfaces = false, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	model->drawfaces = 0;
	for (int i = 0; i < model->faces; i++) {
		bool infront = true;
		for (int j = 0; j < model->face[i].vertices; j++) {
			if (model->vertex[model->face[i].vertex[j]].q <= 0.0f) {
				infront = false;
				break;
			}
		}
		if (!infront) {
			continue;
		}

		vec2 s0 = model->vertex[model->face[i].vertex[0]].spos;
		vec2 s1 = model->vertex[model->face[i].vertex[1]].spos;
		vec2 s2 = model->vertex[model->face[i].vertex[2]].spos;
		// (s1 - s0) × (s2 - s0). Same sign as the face normal's z in this
		// y-down, +z-away frame, so front faces come out negative.
		float winding = cross(s1 - s0, s2 - s0);
		model->face[i].frontfacing = winding < 0;
		if (model->face[i].frontfacing || backfaces) {
			model->face[i].depth = 0;
			for (int j = 0; j < model->face[i].vertices; j++) {
				model->face[i].depth += model->vertex[model->face[i].vertex[j]].rpos.z;
			}
			model->face[i].depth /= model->face[i].vertices;
			model->drawface[model->drawfaces] = i;
			model->drawfaces++;
		}
	}
	if (model->drawfaces > 1) {
		RETRO_QuickSort(model->drawface, 0, model->drawfaces - 1, RETRO_FaceDepthBefore, model);
	}
}

#endif
