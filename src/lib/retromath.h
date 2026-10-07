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
	RETRO_RotateModel(rotate(ax, ay, az), model);
}

// p' = R p + t, the translation half of a model's placement. It goes on the
// rotated coordinates, so the model turns about its own center and is then
// carried to where it stands. Added to the model's own vertices instead it
// would be rotated too, and the model would swing around the origin.
//
// RETRO_ProjectModel's screen center cannot stand in for this. That offset is
// in pixels, applied after the divide, so it neither shrinks with distance nor
// moves the model in z at all.
inline void RETRO_TranslateModel(float tx, float ty, float tz, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].rpos += vec3{ tx, ty, tz };
	}
}

// The pinhole projection's defaults, see RETRO_ProjectVertex. A demo whose
// camera does not fit it - one that wants a focal length per axis, or a depth
// it works out for itself - writes that formula out with what it has.
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
// a rotation (RETRO_RotateUnitVector). RETRO_RotatedDot reads the rotated
// slot, because the shaded renderers have already turned the model. The helpers below read and write the authored slot, which is
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

//
// The convex outline of count points, as the indices of its corners in turn,
// wrapped one corner at a time: from the leftmost, each next corner is the
// point every other lies to one side of, the farthest when several line up.
// Duplicates share a position, so the outline is closed by position. None
// when it would take more than maxcorners corners.
//
inline int RETRO_ConvexOutline(const vec2 *point, int count, int *outline, int maxcorners)
{
	int start = 0;
	for (int i = 1; i < count; i++) {
		if (point[i].x < point[start].x || (point[i].x == point[start].x && point[i].y < point[start].y)) start = i;
	}

	int corners = 0;
	int current = start;
	do {
		if (corners == maxcorners) return 0;
		outline[corners++] = current;
		int next = start;
		for (int i = 0; i < count; i++) {
			vec2 a = point[next] - point[current];
			vec2 b = point[i] - point[current];
			float turn = cross(a, b);
			if (turn < 0 || (turn == 0 && dot(b, b) > dot(a, a))) next = i;
		}
		current = next;
	} while (!(point[current] == point[start]));
	return corners;
}

//
// Where a ray meets a sphere: the nearer root of |O + tD - C|² = r², D unit
//
// A root counts once it is past neart, which keeps a ray leaving a surface
// from meeting that surface again at its own origin. allowinside also returns
// the far root when the near one is behind the origin, which a reflection
// bounce needs when it starts fractionally inside the sphere it just left -
// but a shadow ray, always cast from outside every sphere, must not get: a
// graze it should clear would otherwise read as a hit on the sphere's far
// side.
//
inline bool RETRO_IntersectSphere(vec3 origin, vec3 direction, vec3 center, float radius, float neart, float &t, bool allowinside = true)
{
	vec3 oc = origin - center;
	float b = dot(direction, oc);
	float c = dot(oc, oc) - radius * radius;
	float disc = b * b - c;
	if (disc < 0.0f) return false;

	float root = sqrtf(disc);
	float tt = -b - root;
	if (tt <= neart) {
		if (!allowinside) return false;
		tt = -b + root;
	}
	if (tt <= neart) return false;

	t = tt;
	return true;
}

// Integer hash of a grid position. The same (x, y) always gives the same bits
// and neighboring positions give unrelated ones, so a pattern built from it
// repeats exactly and stays anchored to its grid. The odd multipliers spread
// each coordinate over the word; the xor-shifts fold the well-mixed high bits
// back into the low ones a caller masks off. Unsigned arithmetic keeps the
// overflow defined.
inline unsigned int RETRO_Hash(int x, int y)
{
	unsigned int hash = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
	hash = (hash ^ (hash >> 13)) * 1274126177u;
	return hash ^ (hash >> 16);
}

// The hash of a grid position as a fraction, in [0, 1]
inline float RETRO_HashUnit(int x, int y)
{
	return (RETRO_Hash(x, y) & 65535u) / 65535.0f;
}

// Value noise, in [0, 1]: the fractions hashed at the four grid points around
// (x, y), blended across the cell. The blend rides on smoothstep, flat at
// both ends, so the noise crosses from one cell into the next without a
// crease. One grid unit is one feature; a caller scales x and y for the size
// it wants and sums octaves for detail.
inline float RETRO_ValueNoise(float x, float y)
{
	int ix = (int)floorf(x), iy = (int)floorf(y);
	float fx = smoothstep(0.0f, 1.0f, x - ix);
	float fy = smoothstep(0.0f, 1.0f, y - iy);
	float a = RETRO_HashUnit(ix, iy), b = RETRO_HashUnit(ix + 1, iy);
	float c = RETRO_HashUnit(ix, iy + 1), d = RETRO_HashUnit(ix + 1, iy + 1);
	return mix(mix(a, b, fx), mix(c, d, fx), fy);
}

//
// Catmull-Rom spline: the cubic from p1 at t = 0 to p2 at t = 1, leaving p1
// along (p2 - p0) / 2 and arriving at p2 along (p3 - p1) / 2. Pieces taken
// from overlapping runs of four points share the tangent where they meet, so
// the curve has no corner and no jump in speed there. The basis matrix
// multiplied out:
//
//   C(t) = ½ (2 p1 + (p2 − p0) t + (2 p0 − 5 p1 + 4 p2 − p3) t²
//             + (3 p1 − p0 − 3 p2 + p3) t³)
//
inline vec2 RETRO_CatmullRom(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t)
{
	float t2 = t * t, t3 = t2 * t;
	return (p1 * 2.0f + (p2 - p0) * t
		+ (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
		+ (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
}

inline vec3 RETRO_CatmullRom(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t)
{
	float t2 = t * t, t3 = t2 * t;
	return (p1 * 2.0f + (p2 - p0) * t
		+ (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
		+ (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
}

// The spline's derivative along t, the direction the curve heads at t
inline vec3 RETRO_CatmullRomDerivative(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t)
{
	return ((p2 - p0)
		+ (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * (2 * t)
		+ (p1 * 3.0f - p0 - p2 * 3.0f + p3) * (3 * t * t)) * 0.5f;
}

#endif
