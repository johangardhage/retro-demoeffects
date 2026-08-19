//
// Rubber vector
//
// A filled cube bent by three travelling sine waves before the usual rotation
// and perspective projection.  assets/rubbercubequads.obj is an ordinary unit
// cube whose six quads arrive already subdivided into grids, and the
// subdivision is what matters: it lets the faces themselves bow in and out
// instead of merely carrying the cube's eight corners along.  The result is the
// curved, low-poly sheet appearance of the classic Amiga rubber-vector effect.
//
// The deformation reads nothing but a vertex's rest position, so this demo need
// not know how finely the asset is divided, nor which vertices a face is built
// from.  That is also what keeps the cube watertight: the seams where the
// cube's faces meet carry coincident vertices belonging to different grids, and
// equal positions are displaced equally.
//
// A deformed quad is no longer planar, and RETRO_InitializeFaceNormals takes a
// face normal from the first of the two triangles it is drawn as, so each quad
// is shaded and culled by one half of itself.  At this subdivision a quad
// covers a few pixels and the seam does not show.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define ROTATION_SPEED 0.9f   // radians per second, about the middle axis
#define ROTATION_SPREAD 0.3f  // the other two turn this much slower and faster
#define RUBBER_SPEED 2.4f     // deformation phase per second, of the middle wave
#define RUBBER_SPREAD 0.3f    // and the same spread across the three waves
#define RUBBER_AMOUNT 0.38f   // displacement in model units
#define RUBBER_WAVE1 1.5f     // radians a wave's phase turns per model unit along
#define RUBBER_WAVE2 1.0f     // the next axis, and along the one after that

static Model3D *Rubber;
static Vertex RestVertex[RETRO_MAX_VERTICES];

void DEMO_Render(double deltatime)
{
	static float ax, ay, az, phasex, phasey, phasez;
	ax = fmod(ax + deltatime * ROTATION_SPEED * (1 - ROTATION_SPREAD), 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED * (1 + ROTATION_SPREAD), 2 * M_PI);
	phasex = fmod(phasex + deltatime * RUBBER_SPEED * (1 - RUBBER_SPREAD), 2 * M_PI);
	phasey = fmod(phasey + deltatime * RUBBER_SPEED, 2 * M_PI);
	phasez = fmod(phasez + deltatime * RUBBER_SPEED * (1 + RUBBER_SPREAD), 2 * M_PI);

	// Each axis keeps its own wrapped phase so sin(k * phase) does not jump
	// when k is not an integer.
	for (int i = 0; i < Rubber->vertices; i++) {
		const Vertex &v = RestVertex[i];
		Rubber->vertex[i].x = v.x + RUBBER_AMOUNT * sin(phasex + RUBBER_WAVE1 * v.y + RUBBER_WAVE2 * v.z);
		Rubber->vertex[i].y = v.y + RUBBER_AMOUNT * sin(phasey + RUBBER_WAVE1 * v.z + RUBBER_WAVE2 * v.x);
		Rubber->vertex[i].z = v.z + RUBBER_AMOUNT * sin(phasez + RUBBER_WAVE1 * v.x + RUBBER_WAVE2 * v.y);
	}

	// The face normals the loader took describe the cube at rest, so they are
	// taken again now that the wave has been written into it
	RETRO_InitializeFaceNormals(Rubber);

	RETRO_RotateModel(ax, ay, az, Rubber);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Rubber);
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT, Rubber);
}

void DEMO_Initialize(void)
{
	RETRO_CreateMattePalette();

	Rubber = RETRO_Load3DModel("assets/rubbercubequads.obj");
	Rubber->c = RETRO_PHONG_OFFSET;
	Rubber->cintensity = RETRO_PHONG_SHADES;
	for (int i = 0; i < Rubber->vertices; i++) RestVertex[i] = Rubber->vertex[i];

	RETRO_InitializeLightSource(0, 0, -1);
}
