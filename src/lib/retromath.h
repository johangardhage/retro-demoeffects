//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMATH_H_
#define _RETROMATH_H_

#include "retromodel.h"

// Pinhole projection. Depth is s*rz + eye, and the screen point is
//
//   q  = 1 / depth
//   sx = cx + s * rx * eye * q
//   sy = cy + s * ry * eye * q
//
// A demo that works out a projected vertex itself has to use the same eye.
#define RETRO_PROJECTION_EYE 250

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
		model->normal[i].rnx = model->normal[i].nx * model->matrix[0][0] + model->normal[i].ny * model->matrix[0][1] + model->normal[i].nz * model->matrix[0][2];
		model->normal[i].rny = model->normal[i].nx * model->matrix[1][0] + model->normal[i].ny * model->matrix[1][1] + model->normal[i].nz * model->matrix[1][2];
		model->normal[i].rnz = model->normal[i].nx * model->matrix[2][0] + model->normal[i].ny * model->matrix[2][1] + model->normal[i].nz * model->matrix[2][2];
	}
}

void RETRO_RotateFaceNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		model->face[i].facenormal.rnx = model->face[i].facenormal.nx * model->matrix[0][0] + model->face[i].facenormal.ny * model->matrix[0][1] + model->face[i].facenormal.nz * model->matrix[0][2];
		model->face[i].facenormal.rny = model->face[i].facenormal.nx * model->matrix[1][0] + model->face[i].facenormal.ny * model->matrix[1][1] + model->face[i].facenormal.nz * model->matrix[1][2];
		model->face[i].facenormal.rnz = model->face[i].facenormal.nx * model->matrix[2][0] + model->face[i].facenormal.ny * model->matrix[2][1] + model->face[i].facenormal.nz * model->matrix[2][2];
	}
}

void RETRO_ProjectModel(float scale = RETRO_PROJECTION_SCALE, int x = (RETRO_WIDTH / 2), int y = (RETRO_HEIGHT / 2), Model3D *model = NULL, int eye = RETRO_PROJECTION_EYE)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		float depth = scale * model->vertex[i].rz + eye;
		if (depth <= 1.0f) {
			model->vertex[i].q = 0.0f;
			model->vertex[i].sx = x;
			model->vertex[i].sy = y;
		} else {
			model->vertex[i].q = 1.0f / depth;
			model->vertex[i].sx = x + scale * model->vertex[i].rx * eye * model->vertex[i].q;
			model->vertex[i].sy = y + scale * model->vertex[i].ry * eye * model->vertex[i].q;
		}
	}
}

void RETRO_RotateModel(float ax, float ay, float az, Model3D *model = NULL)
{
	RETRO_InitializeRotationMatrix(ax, ay, az, model);
	RETRO_RotateVertices(model);
	RETRO_RotateVertexNormals(model);
	RETRO_RotateFaceNormals(model);
}

void RETRO_ProjectVertex(Vertex *vertex, float scale = RETRO_PROJECTION_SCALE, int x = (RETRO_WIDTH / 2), int y = (RETRO_HEIGHT / 2), int eye = RETRO_PROJECTION_EYE)
{
	float depth = scale * vertex->rz + eye;
	if (depth <= 1.0f) {
		vertex->q = 0.0f;
		vertex->sx = x;
		vertex->sy = y;
	} else {
		vertex->q = 1.0f / depth;
		vertex->sx = x + scale * vertex->rx * eye * vertex->q;
		vertex->sy = y + scale * vertex->ry * eye * vertex->q;
	}
}

// Sequential Rx, Ry, Rz by the same (cos, sin) pair. Each plane map scales
// that plane by r = √(cos²+sin²) and leaves its axis alone. When r ≠ 1
// (dottorus2's SQUASH) the composition is not in SO(3).
void RETRO_RotateVertex(Vertex *vertex, float cosa, float sina)
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

void RETRO_RotateNormal(Normal *normal, float ax, float ay, float az)
{
	// Rotate around x axis
	normal->rny = normal->ny * cos(ax) - normal->nz * sin(ax);
	normal->rnz = normal->ny * sin(ax) + normal->nz * cos(ax);

	// Rotate around y axis
	normal->rnx = normal->nx * cos(ay) + normal->rnz * sin(ay);
	normal->rnz = normal->nx * -sin(ay) + normal->rnz * cos(ay);

	// Rotate around z axis
	float tmpx = normal->rnx * cos(az) - normal->rny * sin(az);
	normal->rny = normal->rnx * sin(az) + normal->rny * cos(az);
	normal->rnx = tmpx;
}

// (N1 · N2) / (|N1| |N2|), the cosine of the angle between the rotated normals.
float RETRO_DotProduct(Normal n1, Normal n2)
{
	float lengths = n1.nn * n2.nn;
	if (lengths <= 0.0f) return 0.0f;
	return (n1.rnx * n2.rnx + n1.rny * n2.rny + n1.rnz * n2.rnz) / lengths;
}

void RETRO_QuickSort(Model3D *model, int lo, int hi)
{
	int i = lo;
	int j = hi;
	float z = model->face[model->visibleface[(lo + hi) / 2]].z;

	while (i <= j) {
		while (model->face[model->visibleface[i]].z > z) {
			i++;
		}
		while (model->face[model->visibleface[j]].z < z) {
			j--;
		}

		if (i <= j) {
			SWAP(model->visibleface[i], model->visibleface[j]);
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
// Painter's algorithm: sort remaining faces by mean rz, far to near.
void RETRO_SortVisibleFaces(Model3D *model = NULL, bool all = false)
{
	model = model ? model : RETRO_Get3DModel();

	model->visiblefaces = 0;
	for (int i = 0; i < model->faces; i++) {
		bool infront = true;
		for (int j = 0; j < model->face[i].vertices; j++) {
			if (model->vertex[model->face[i].vertex[j]].q <= 0.0f) {
				infront = false;
				break;
			}
		}
		if (!infront) {
			model->face[i].visible = false;
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
		model->face[i].visible = cross < 0;
		if (model->face[i].visible || all) {
			model->face[i].z = 0;
			for (int j = 0; j < model->face[i].vertices; j++) {
				model->face[i].z += model->vertex[model->face[i].vertex[j]].rz;
			}
			model->face[i].z /= model->face[i].vertices;
			model->visibleface[model->visiblefaces] = i;
			model->visiblefaces++;
		}
	}
	if (model->visiblefaces > 1) {
		RETRO_QuickSort(model, 0, model->visiblefaces - 1);
	}
}

void RETRO_SortAllFaces(Model3D *model = NULL)
{
	RETRO_SortVisibleFaces(model, true);
}

#endif
