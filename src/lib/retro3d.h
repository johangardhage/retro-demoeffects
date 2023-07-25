//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETRO3D_H_
#define _RETRO3D_H_

#include "retro.h"
#include "retro3dmodel.h"

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

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].rnx = model->vertex[i].nx * model->matrix[0][0] + model->vertex[i].ny * model->matrix[1][0] + model->vertex[i].nz * model->matrix[2][0];
		model->vertex[i].rny = model->vertex[i].nx * model->matrix[0][1] + model->vertex[i].ny * model->matrix[1][1] + model->vertex[i].nz * model->matrix[2][1];
		model->vertex[i].rnz = model->vertex[i].nx * model->matrix[0][2] + model->vertex[i].ny * model->matrix[1][2] + model->vertex[i].nz * model->matrix[2][2];
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

void RETRO_ProjectVertices(float scale, int x = 0, int y = 0, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();
	x = x ? x : (RETRO_WIDTH / 2);
	y = y ? y : (RETRO_HEIGHT / 2);

	for (int i = 0; i < model->vertices; i++) {
		int eye = 250;
		model->vertex[i].sx = x + (scale * model->vertex[i].rx * eye) / (scale * model->vertex[i].rz + eye);
		model->vertex[i].sy = y + (scale * model->vertex[i].ry * eye) / (scale * model->vertex[i].rz + eye);
	}
}

void RETRO_InitializeVertexNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		// Calculate normal
		model->vertex[i].nx = model->vertex[i].x;
		model->vertex[i].ny = model->vertex[i].y;
		model->vertex[i].nz = model->vertex[i].z;

		// Calculate the length of the normal (used to scale it to a unit normal)
		model->vertex[i].nn = sqrt(model->vertex[i].nx * model->vertex[i].nx + model->vertex[i].ny * model->vertex[i].ny + model->vertex[i].nz * model->vertex[i].nz);
	}
}

void RETRO_InitializeFaceNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		// Calculate vectors
		float x1 = model->vertex[model->face[i].vertex[0]].x - model->vertex[model->face[i].vertex[1]].x;
		float y1 = model->vertex[model->face[i].vertex[0]].y - model->vertex[model->face[i].vertex[1]].y;
		float z1 = model->vertex[model->face[i].vertex[0]].z - model->vertex[model->face[i].vertex[1]].z;
		float x2 = model->vertex[model->face[i].vertex[0]].x - model->vertex[model->face[i].vertex[2]].x;
		float y2 = model->vertex[model->face[i].vertex[0]].y - model->vertex[model->face[i].vertex[2]].y;
		float z2 = model->vertex[model->face[i].vertex[0]].z - model->vertex[model->face[i].vertex[2]].z;

		// Calculate normal (using cross product)
		model->face[i].nx = y1 * z2 - z1 * y2;
		model->face[i].ny = z1 * x2 - x1 * z2;
		model->face[i].nz = x1 * y2 - y1 * x2;

		// Calculate the length of the normal (used to scale it to a unit normal)
		model->face[i].nn = sqrt(model->face[i].nx * model->face[i].nx + model->face[i].ny * model->face[i].ny + model->face[i].nz * model->face[i].nz);
	}
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
		model->face[i].znm = (model->vertex[model->face[i].vertex[1]].sx - model->vertex[model->face[i].vertex[0]].sx) * (model->vertex[model->face[i].vertex[0]].sy - model->vertex[model->face[i].vertex[2]].sy) - (model->vertex[model->face[i].vertex[1]].sy - model->vertex[model->face[i].vertex[0]].sy) * (model->vertex[model->face[i].vertex[0]].sx - model->vertex[model->face[i].vertex[2]].sx);
		if (model->face[i].znm > 0 || all) {
//			model->face[i].z = model->vertex[model->face[i].vertex[0]].rz + model->vertex[model->face[i].vertex[1]].rz + model->vertex[model->face[i].vertex[2]].rz;
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
