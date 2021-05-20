//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETRO3D2_H_
#define _RETRO3D2_H_

#include "retro.h"
#include "retro3dmodel.h"

// Maximum # of degrees for sin and cos tables
#define MAXDEGREES 720

struct Point3Df {
	float x, y, z;
};

struct Point3D {
	int x, y, z;
};

float CosRotationTable[MAXDEGREES]; // Cos rotation lookup table
float SinRotationTable[MAXDEGREES]; // Sin rotation lookup table

void RETRO_InitializeRotationTables(void)
{
	for(int i = 0; i < MAXDEGREES; i++)
	{
		CosRotationTable[i] = cos((float)i * 360 / MAXDEGREES * 3.14159265 / 180.0);
		SinRotationTable[i] = sin((float)i * 360 / MAXDEGREES * 3.14159265 / 180.0);
	}
}

void RETRO_RotateVertices(int ax, int ay, int az, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	float sinxangle = SinRotationTable[ax];
	float cosxangle = CosRotationTable[ax];
	float sinyangle = SinRotationTable[ay];
	float cosyangle = CosRotationTable[ay];
	float sinzangle = SinRotationTable[az];
	float coszangle = CosRotationTable[az];

	float nx, ny, nz;

	for(int i = 0; i < model->vertices; i++) {
		Vertex *vertex = &model->vertex[i];

		// Rotate around the x-axis
		vertex->rz = vertex->y * cosxangle - vertex->z * sinxangle;
		vertex->ry = vertex->y * sinxangle + vertex->z * cosxangle;
		vertex->rx = vertex->x;

		// Rotate around the y-axis
		nx = vertex->rx * cosyangle - vertex->rz * sinyangle;
		nz = vertex->rx * sinyangle + vertex->rz * cosyangle;
		vertex->rx = nx;
		vertex->rz = nz;

		// Rotate around the z-axis
		nx = vertex->rx * coszangle - vertex->ry * sinzangle;
		ny = vertex->rx * sinzangle + vertex->ry * coszangle;
		vertex->rx = nx;
		vertex->ry = ny;
	}
}

Point3D RETRO_Rotate(Point3D point, float cosvalue, float sinvalue)
{
	Point3D rotated;

	// Rotate on X
	rotated.y = point.y * cosvalue - point.z * sinvalue;
	rotated.z = point.y * sinvalue + point.z * cosvalue;

	// Rotate on Y
	rotated.x = point.x * cosvalue + rotated.z * sinvalue;
	rotated.z = -point.x * sinvalue + rotated.z * cosvalue;

	// Rotate on Z
	int tmpx = rotated.x * cosvalue - rotated.y * sinvalue;
	rotated.y = rotated.x * sinvalue + rotated.y * cosvalue;
	rotated.x = tmpx;

	return rotated;
}

Point3Df RETRO_Rotate(Point3Df point, float cosvalue, float sinvalue)
{
	Point3Df rotated;

	// Rotate on X
	rotated.y = point.y * cosvalue - point.z * sinvalue;
	rotated.z = point.y * sinvalue + point.z * cosvalue;

	// Rotate on Y
	rotated.x = point.x * cosvalue + rotated.z * sinvalue;
	rotated.z = -point.x * sinvalue + rotated.z * cosvalue;

	// Rotate on Z
	float tmpx = rotated.x * cosvalue - rotated.y * sinvalue;
	rotated.y = rotated.x * sinvalue + rotated.y * cosvalue;
	rotated.x = tmpx;

	return rotated;
}

Point3Df RETRO_Rotate(Point3Df point, float ax, float ay, float az)
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

Point2D RETRO_Project(Point3D point, int eyedistance, int scale)
{
	Point2D projected;

	projected.x = RETRO_WIDTH / 2 + (int)ceil(scale * point.x * eyedistance / (scale * point.z + eyedistance));
	projected.y = RETRO_HEIGHT / 2 + (int)ceil(scale * point.y * eyedistance / (scale * point.z + eyedistance));

	return projected;
}

Point2D RETRO_Project(Point3Df point, int eyedistance, int scale)
{
	Point2D projected;

	projected.x = RETRO_WIDTH / 2 + (int)ceil(scale * point.x * eyedistance / (scale * point.z + eyedistance));
	projected.y = RETRO_HEIGHT / 2 + (int)ceil(scale * point.y * eyedistance / (scale * point.z + eyedistance));

	return projected;
}

double RETRO_Distance(Point3D point1, Point3D point2)
{
	return (point1.x - point2.x) * (point1.x - point2.x) + (point1.y - point2.y) * (point1.y - point2.y) + (point1.z - point2.z) * (point1.z - point2.z);
}

#endif
