//
// Jelly cube
//
// A subdivided cube that squashes as a volume rather than bending as a
// mesh: rubbervector2.cpp bends it with three traveling sine waves,
// rubbervector3.cpp twists a Glenz-shaded column of triangles, and
// rubbervector4.cpp gets the classic Amiga look without deforming a mesh
// at all, by scanline-multiplexing a ring of rigid rendered frames. This
// one scales. The three axes take a pulse 120° apart
//
//   s_x = 1 + A sin(φ)
//   s_y = 1 + A sin(φ + 2π/3)
//   s_z = 1 + A sin(φ + 4π/3)
//
// so the box is always stretching on one axis while it flattens on the
// others, the way a cube of jelly does under a tap. A traveling bulge
// then rides the rest y
//
//   b = 1 + B sin(k y_rest + 2φ)
//
// and scales the two horizontal axes, so a wave of fatness walks the
// cube while it pulses. subcubequads.obj is already a grid
// per face; the bulge would be invisible on eight corners. Face normals
// are taken again after the scale, because the rest normals describe
// the cube at rest. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 0.85f // radians a second, about the middle axis
#define ROTATION_SPREAD 0.28f // the other two turn this much slower and faster
#define PULSE_SPEED 2.1f // radians of the squash per second
#define PULSE_AMOUNT 0.16f // how far an axis stretches from 1
#define BULGE_AMOUNT 0.12f // extra scale the traveling wave adds
#define BULGE_WAVE 1.8f // radians of that wave per model unit of rest y

static vec3 RestPos[RETRO_MAX_VERTICES];

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	float ax = fmod(time.total * ROTATION_SPEED * (1 - ROTATION_SPREAD), 2 * M_PI);
	float ay = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time.total * ROTATION_SPEED * (1 + ROTATION_SPREAD), 2 * M_PI);
	float pulse = fmod(time.total * PULSE_SPEED, 2 * M_PI);
	float bulge = fmod(time.total * PULSE_SPEED * 2, 2 * M_PI);

	float sx = 1 + PULSE_AMOUNT * sin(pulse);
	float sy = 1 + PULSE_AMOUNT * sin(pulse + 2 * M_PI / 3);
	float sz = 1 + PULSE_AMOUNT * sin(pulse + 4 * M_PI / 3);

	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < model->vertices; i++) {
		const vec3 &v = RestPos[i];
		float b = 1 + BULGE_AMOUNT * (float)sin(BULGE_WAVE * v.y + bulge);
		model->vertex[i].pos = { v.x * sx * b, v.y * sy, v.z * sz * b };
	}

	RETRO_InitializeFaceNormals(model);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	RETRO_CreateMattePalette(RETRO_SPRINGGREEN);

	Model3D *model = RETRO_Load3DModel("assets/subcubequads.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;
	for (int i = 0; i < model->vertices; i++) {
		RestPos[i] = model->vertex[i].pos;
	}
}
