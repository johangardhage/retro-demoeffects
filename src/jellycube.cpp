//
// Jelly cube
//
// A subdivided cube that squashes as a volume rather than bending as a
// mesh. rubbervector.cpp is scanline-multiplexed delay; rubbervector2.cpp
// adds three travelling sines to the rest position. This one scales.
// The three axes take a pulse 120° apart
//
//   s_x = 1 + A sin(φ)
//   s_y = 1 + A sin(φ + 2π/3)
//   s_z = 1 + A sin(φ + 4π/3)
//
// so the box is always stretching on one axis while it flattens on the
// others, the way a cube of jelly does under a tap. A travelling bulge
// then rides the rest y
//
//   b = 1 + B sin(k y_rest + 2φ)
//
// and scales the two horizontal axes, so a wave of fatness walks the
// cube while it pulses. assets/rubbercubequads.obj is already a grid
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
#define BULGE_AMOUNT 0.12f // extra scale the travelling wave adds
#define BULGE_WAVE 1.8f // radians of that wave per model unit of rest y

static Model3D *Jelly;
static Vertex RestVertex[RETRO_MAX_VERTICES];

void DEMO_Render(double deltatime)
{
	static float ax, ay, az, pulse, bulge;
	ax = fmod(ax + deltatime * ROTATION_SPEED * (1 - ROTATION_SPREAD), 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED * (1 + ROTATION_SPREAD), 2 * M_PI);
	pulse = fmod(pulse + deltatime * PULSE_SPEED, 2 * M_PI);
	bulge = fmod(bulge + deltatime * PULSE_SPEED * 2, 2 * M_PI);

	float sx = 1 + PULSE_AMOUNT * sin(pulse);
	float sy = 1 + PULSE_AMOUNT * sin(pulse + 2 * M_PI / 3);
	float sz = 1 + PULSE_AMOUNT * sin(pulse + 4 * M_PI / 3);

	for (int i = 0; i < Jelly->vertices; i++) {
		const Vertex &v = RestVertex[i];
		float b = 1 + BULGE_AMOUNT * sin(BULGE_WAVE * v.y + bulge);
		Jelly->vertex[i].x = v.x * sx * b;
		Jelly->vertex[i].y = v.y * sy;
		Jelly->vertex[i].z = v.z * sz * b;
	}

	RETRO_InitializeFaceNormals(Jelly);

	RETRO_RotateModel(ax, ay, az, Jelly);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Jelly);
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT, Jelly);
}

void DEMO_Initialize(void)
{
	RETRO_CreateMattePalette(RETRO_SPRINGGREEN);

	Jelly = RETRO_Load3DModel("assets/rubbercubequads.obj");
	Jelly->c = RETRO_PHONG_OFFSET;
	Jelly->shades = RETRO_PHONG_SHADES;
	for (int i = 0; i < Jelly->vertices; i++) {
		RestVertex[i] = Jelly->vertex[i];
	}

	RETRO_InitializeLightSource(0, 0, -1);
}
