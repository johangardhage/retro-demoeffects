//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMATH_H_
#define _RETROMATH_H_

#include "retromodel.h"

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

// R = Rz(az) * Ry(ay) * Rx(ax). Applied to column vectors as p' = R p.
void RETRO_InitializeRotationMatrix(float ax, float ay, float az, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	model->matrix[0][0] = cos(az) * cos(ay);
	model->matrix[1][0] = sin(az) * cos(ay);
	model->matrix[2][0] = -sin(ay);
	model->matrix[0][1] = cos(az) * sin(ay) * sin(ax) - sin(az) * cos(ax);
	model->matrix[1][1] = sin(az) * sin(ay) * sin(ax) + cos(ax) * cos(az);
	model->matrix[2][1] = sin(ax) * cos(ay);
	model->matrix[0][2] = cos(az) * sin(ay) * cos(ax) + sin(az) * sin(ax);
	model->matrix[1][2] = sin(az) * sin(ay) * cos(ax) - cos(az) * sin(ax);
	model->matrix[2][2] = cos(ax) * cos(ay);
}

void RETRO_RotateVertices(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].rx = model->vertex[i].x * model->matrix[0][0] + model->vertex[i].y * model->matrix[0][1] + model->vertex[i].z * model->matrix[0][2];
		model->vertex[i].ry = model->vertex[i].x * model->matrix[1][0] + model->vertex[i].y * model->matrix[1][1] + model->vertex[i].z * model->matrix[1][2];
		model->vertex[i].rz = model->vertex[i].x * model->matrix[2][0] + model->vertex[i].y * model->matrix[2][1] + model->vertex[i].z * model->matrix[2][2];
	}
}

void RETRO_RotateVertexNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->normals; i++) {
		model->normal[i].rx = model->normal[i].x * model->matrix[0][0] + model->normal[i].y * model->matrix[0][1] + model->normal[i].z * model->matrix[0][2];
		model->normal[i].ry = model->normal[i].x * model->matrix[1][0] + model->normal[i].y * model->matrix[1][1] + model->normal[i].z * model->matrix[1][2];
		model->normal[i].rz = model->normal[i].x * model->matrix[2][0] + model->normal[i].y * model->matrix[2][1] + model->normal[i].z * model->matrix[2][2];
	}
}

// The face's frame - its normal and the tangent and bitangent built on it.
// All three turn together: T and B are perpendicular to N by construction and
// only stay so in view space if one matrix carries all of them. N goes to the
// flat and env shading; T and B reach the drawers as a TangentFrame, and have
// to turn with the model or the relief stays keyed to the screen.
void RETRO_RotateFaceFrames(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		Direction *direction[2] = { &model->face[i].tangent, &model->face[i].bitangent };
		for (int j = 0; j < 2; j++) {
			direction[j]->rx = direction[j]->x * model->matrix[0][0] + direction[j]->y * model->matrix[0][1] + direction[j]->z * model->matrix[0][2];
			direction[j]->ry = direction[j]->x * model->matrix[1][0] + direction[j]->y * model->matrix[1][1] + direction[j]->z * model->matrix[1][2];
			direction[j]->rz = direction[j]->x * model->matrix[2][0] + direction[j]->y * model->matrix[2][1] + direction[j]->z * model->matrix[2][2];
		}

		model->face[i].facenormal.rx = model->face[i].facenormal.x * model->matrix[0][0] + model->face[i].facenormal.y * model->matrix[0][1] + model->face[i].facenormal.z * model->matrix[0][2];
		model->face[i].facenormal.ry = model->face[i].facenormal.x * model->matrix[1][0] + model->face[i].facenormal.y * model->matrix[1][1] + model->face[i].facenormal.z * model->matrix[1][2];
		model->face[i].facenormal.rz = model->face[i].facenormal.x * model->matrix[2][0] + model->face[i].facenormal.y * model->matrix[2][1] + model->face[i].facenormal.z * model->matrix[2][2];
	}
}

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
void RETRO_ProjectVertex(Vertex *vertex, float scale = RETRO_PROJECTION_SCALE, float cx = (RETRO_WIDTH / 2), float cy = (RETRO_HEIGHT / 2), float eyedistance = RETRO_PROJECTION_EYEDISTANCE)
{
	float depth = scale * vertex->rz + eyedistance;

	if (depth <= 1.0f) {
		vertex->q = 0.0f;
		vertex->sx = cx;
		vertex->sy = cy;
	} else {
		float focal = scale * eyedistance;

		vertex->q = 1.0f / depth;
		vertex->sx = cx + focal * vertex->rx * vertex->q;
		vertex->sy = cy + focal * vertex->ry * vertex->q;
	}
}

void RETRO_ProjectModel(float scale = RETRO_PROJECTION_SCALE, float cx = (RETRO_WIDTH / 2), float cy = (RETRO_HEIGHT / 2), Model3D *model = NULL, float eyedistance = RETRO_PROJECTION_EYEDISTANCE)
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
}

void RETRO_RotateModel(float ax, float ay, float az, Model3D *model = NULL)
{
	RETRO_InitializeRotationMatrix(ax, ay, az, model);
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
void RETRO_TranslateModel(float tx, float ty, float tz, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].rx += tx;
		model->vertex[i].ry += ty;
		model->vertex[i].rz += tz;
	}
}

// Sequential Rx, Ry, Rz by the same (cos, sin) pair, where RETRO_RotateVertex
// below takes an angle per axis. The angle arrives already resolved, so a demo
// can spin from a table. Each plane map scales that plane by r = √(cos²+sin²)
// and leaves its axis alone, so a pair that is not unit squashes the model as
// it turns and the composition is not in SO(3), which is why this is not
// called a rotation.
void RETRO_SpinVertex(Vertex *vertex, float cosa, float sina)
{
	// Rotate around x axis
	vertex->ry = vertex->y * cosa - vertex->z * sina;
	vertex->rz = vertex->y * sina + vertex->z * cosa;

	// Rotate around y axis
	vertex->rx = vertex->x * cosa + vertex->rz * sina;
	vertex->rz = vertex->x * -sina + vertex->rz * cosa;

	// Rotate around z axis
	float tmpx = vertex->rx * cosa - vertex->ry * sina;
	vertex->ry = vertex->rx * sina + vertex->ry * cosa;
	vertex->rx = tmpx;
}

void RETRO_RotateVertex(Vertex *vertex, float ax, float ay, float az)
{
	// Rotate around x axis
	vertex->ry = vertex->y * cos(ax) - vertex->z * sin(ax);
	vertex->rz = vertex->y * sin(ax) + vertex->z * cos(ax);

	// Rotate around y axis
	vertex->rx = vertex->x * cos(ay) + vertex->rz * sin(ay);
	vertex->rz = vertex->x * -sin(ay) + vertex->rz * cos(ay);

	// Rotate around z axis
	float tmpx = vertex->rx * cos(az) - vertex->ry * sin(az);
	vertex->ry = vertex->rx * sin(az) + vertex->ry * cos(az);
	vertex->rx = tmpx;
}

void RETRO_RotateDirection(Direction *direction, float ax, float ay, float az)
{
	// Rotate around x axis
	direction->ry = direction->y * cos(ax) - direction->z * sin(ax);
	direction->rz = direction->y * sin(ax) + direction->z * cos(ax);

	// Rotate around y axis
	direction->rx = direction->x * cos(ay) + direction->rz * sin(ay);
	direction->rz = direction->x * -sin(ay) + direction->rz * cos(ay);

	// Rotate around z axis
	float tmpx = direction->rx * cos(az) - direction->ry * sin(az);
	direction->ry = direction->rx * sin(az) + direction->ry * cos(az);
	direction->rx = tmpx;
}

// D1 · D2, taken on the rotated directions. Both are unit, so this is already
// the cosine of the angle between them and there is nothing to divide out.
float RETRO_DotProduct(Direction d1, Direction d2)
{
	return d1.rx * d2.rx + d1.ry * d2.ry + d1.rz * d2.rz;
}

void RETRO_QuickSort(Model3D *model, int lo, int hi)
{
	int i = lo;
	int j = hi;
	float rz = model->face[model->drawface[(lo + hi) / 2]].rz;

	while (i <= j) {
		while (model->face[model->drawface[i]].rz > rz) {
			i++;
		}
		while (model->face[model->drawface[j]].rz < rz) {
			j--;
		}

		if (i <= j) {
			SWAP(model->drawface[i], model->drawface[j]);
			i++;
			j--;
		}
	}

	if (i < hi) {
		RETRO_QuickSort(model, i, hi);
	}
	if (lo < j) {
		RETRO_QuickSort(model, lo, j);
	}
}

// Drop faces behind the near plane (any vertex with q <= 0). Front-facing is
// the screen-space cross product (s1 - s0) × (s2 - s0). The projection keeps
// view x and y, and the frame is y down with +z away from the viewer, so this
// product carries the sign of the face normal's z: it is negative exactly when
// the outward normal (the right-hand rule on the same winding that
// RETRO_InitializeFaceNormals uses) points back at the viewer.
// Painter's algorithm: the survivors go into drawface sorted by mean rz, far
// to near, which is the list the renderers draw. With backfaces a face goes in
// whether it faces the viewer or not, and its Face::frontfacing says which side
// is showing, so a Glenz or wireframe model can draw both.
//
// A face dropped at the near plane never reaches the winding test, so it never
// reaches drawface either and its frontfacing is left as it stands. There is no
// answer to give: its corners were parked on the principal point with q = 0, so
// the cross product would be meaningless.
void RETRO_SortFaces(Model3D *model = NULL, bool backfaces = false)
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

		float s0x = model->vertex[model->face[i].vertex[0]].sx;
		float s0y = model->vertex[model->face[i].vertex[0]].sy;
		float s1x = model->vertex[model->face[i].vertex[1]].sx;
		float s1y = model->vertex[model->face[i].vertex[1]].sy;
		float s2x = model->vertex[model->face[i].vertex[2]].sx;
		float s2y = model->vertex[model->face[i].vertex[2]].sy;
		// (s1 - s0) × (s2 - s0). Same sign as the face normal's z in this
		// y-down, +z-away frame, so front faces come out negative.
		float cross = (s1x - s0x) * (s2y - s0y) - (s1y - s0y) * (s2x - s0x);
		model->face[i].frontfacing = cross < 0;
		if (model->face[i].frontfacing || backfaces) {
			model->face[i].rz = 0;
			for (int j = 0; j < model->face[i].vertices; j++) {
				model->face[i].rz += model->vertex[model->face[i].vertex[j]].rz;
			}
			model->face[i].rz /= model->face[i].vertices;
			model->drawface[model->drawfaces] = i;
			model->drawfaces++;
		}
	}
	if (model->drawfaces > 1) {
		RETRO_QuickSort(model, 0, model->drawfaces - 1);
	}
}

#endif
