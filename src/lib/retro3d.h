//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETRO3D_H_
#define _RETRO3D_H_

#include "retro.h"
#include "retro3dmodel.h"

struct Point3Df {
	float x, y, z;
};

struct Point3D {
	int x, y, z;
};

void RETRO_RotateMatrix(float ax, float ay, float az, Model3D *model = NULL)
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
		model->vertex[i].rx = model->vertex[i].x * model->matrix[0][0] + model->vertex[i].y * model->matrix[1][0] + model->vertex[i].z * model->matrix[2][0];
		model->vertex[i].ry = model->vertex[i].x * model->matrix[0][1] + model->vertex[i].y * model->matrix[1][1] + model->vertex[i].z * model->matrix[2][1];
		model->vertex[i].rz = model->vertex[i].x * model->matrix[0][2] + model->vertex[i].y * model->matrix[1][2] + model->vertex[i].z * model->matrix[2][2];
	}
}

void RETRO_RotateVertexNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->normals; i++) {
		model->normal[i].rnx = model->normal[i].nx * model->matrix[0][0] + model->normal[i].ny * model->matrix[1][0] + model->normal[i].nz * model->matrix[2][0];
		model->normal[i].rny = model->normal[i].nx * model->matrix[0][1] + model->normal[i].ny * model->matrix[1][1] + model->normal[i].nz * model->matrix[2][1];
		model->normal[i].rnz = model->normal[i].nx * model->matrix[0][2] + model->normal[i].ny * model->matrix[1][2] + model->normal[i].nz * model->matrix[2][2];
	}
}

void RETRO_RotateFaceNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		model->face[i].rnx = model->face[i].nx * model->matrix[0][0] + model->face[i].ny * model->matrix[1][0] + model->face[i].nz * model->matrix[2][0];
		model->face[i].rny = model->face[i].nx * model->matrix[0][1] + model->face[i].ny * model->matrix[1][1] + model->face[i].nz * model->matrix[2][1];
		model->face[i].rnz = model->face[i].nx * model->matrix[0][2] + model->face[i].ny * model->matrix[1][2] + model->face[i].nz * model->matrix[2][2];
	}
}

void RETRO_ProjectVertices(float scale = 50, int eye = 256, int x = (RETRO_WIDTH / 2), int y = (RETRO_HEIGHT / 2), Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].sx = x + (scale * model->vertex[i].rx * eye) / (scale * model->vertex[i].rz + eye);
		model->vertex[i].sy = y + (scale * model->vertex[i].ry * eye) / (scale * model->vertex[i].rz + eye);
	}
}

Point2D RETRO_ProjectPoint(Point3Df point, float scale = 50, int eye = 256, int x = (RETRO_WIDTH / 2), int y = (RETRO_HEIGHT / 2))
{
	Point2D projected;

	projected.x = x + (scale * point.x * eye) / (scale * point.z + eye);
	projected.y = y + (scale * point.y * eye) / (scale * point.z + eye);

	return projected;
}

Point3Df RETRO_RotatePoint(Point3Df point, float acos, float asin)
{
	Point3Df rotated;

	// Rotate on X
	rotated.y = point.y * acos - point.z * asin;
	rotated.z = point.y * asin + point.z * acos;

	// Rotate on Y
	rotated.x = point.x * acos + rotated.z * asin;
	rotated.z = -point.x * asin + rotated.z * acos;

	// Rotate on Z
	float tmpx = rotated.x * acos - rotated.y * asin;
	rotated.y = rotated.x * asin + rotated.y * acos;
	rotated.x = tmpx;

	return rotated;
}

Point3Df RETRO_RotatePoint(Point3Df point, float ax, float ay, float az)
{
	Point3Df rotated;

	// Rotate on X
	rotated.y = point.y * cos(ax) - point.z * sin(ax);
	rotated.z = point.y * sin(ax) + point.z * cos(ax);

	// Rotate on Y
	rotated.x = point.x * cos(ay) + rotated.z * sin(ay);
	rotated.z = -point.x * sin(ay) + rotated.z * cos(ay);

	// Rotate on Z
	float tmpx = rotated.x * cos(az) - rotated.y * sin(az);
	rotated.y = rotated.x * sin(az) + rotated.y * cos(az);
	rotated.x = tmpx;

	return rotated;
}

void RETRO_RotateLightSource(float ax, float ay, float az)
{
	float nx, ny, nz;

	// Rotate around the x-axis
	LightSource.nz = LightSource.y * cos(ax) - LightSource.z * sin(ax);
	LightSource.ny = LightSource.y * sin(ax) + LightSource.z * cos(ax);
	LightSource.nx = LightSource.x;

	// Rotate around the y-axis
	nx = LightSource.nx * cos(ay) - LightSource.nz * sin(ay);
	nz = LightSource.nx * sin(ay) + LightSource.nz * cos(ay);
	LightSource.nx = nx;
	LightSource.nz = nz;

	// Rotate around the z-axis
	nx = LightSource.nx * cos(az) - LightSource.ny * sin(az);
	ny = LightSource.nx * sin(az) + LightSource.ny * cos(az);

	// Reverse the direction of the normals for lightsource shading
	LightSource.nx = -nx;
	LightSource.ny = -ny;
	LightSource.nz = -nz;
}

void RETRO_InitializeLightSource(float x, float y, float z)
{
	LightSource.x = x;
	LightSource.y = y;
	LightSource.z = z;

	// Calculate the length of the vector
	LightSource.nn = sqrt(x * x + y * y + z * z);

	// Rotate it once
	RETRO_RotateLightSource(0, 0, 0);
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

void RETRO_SortVisibleFaces(Model3D *model = NULL, bool all = false)
{
	model = model ? model : RETRO_Get3DModel();

	model->visiblefaces = 0;
	for (int i = 0; i < model->faces; i++) {
		model->face[i].znm = (model->vertex[model->face[i].vertex[1]].sx - model->vertex[model->face[i].vertex[0]].sx) *
							 (model->vertex[model->face[i].vertex[0]].sy - model->vertex[model->face[i].vertex[2]].sy) -
							 (model->vertex[model->face[i].vertex[1]].sy - model->vertex[model->face[i].vertex[0]].sy) *
							 (model->vertex[model->face[i].vertex[0]].sx - model->vertex[model->face[i].vertex[2]].sx);
		if (model->face[i].znm > 0 || all) {
			// model->face[i].z = model->vertex[model->face[i].vertex[0]].rz + model->vertex[model->face[i].vertex[1]].rz + model->vertex[model->face[i].vertex[2]].rz;
			model->face[i].z = 0;
			for (int j = 0; j < model->face[i].vertices; j++) {
				model->face[i].z += model->vertex[model->face[i].vertex[j]].rz;
			}
			model->visibleface[model->visiblefaces] = i;
			model->visiblefaces++;
		}
	}
	RETRO_QuickSort(model, 0, model->visiblefaces - 1);
}

void RETRO_SortAllFaces(Model3D *model = NULL)
{
	RETRO_SortVisibleFaces(model, true);
}

#endif
