//
// Glenz cube
//
// Additive palette indices, both sides of every face. A pixel that
// several faces cover becomes
//
//   C' = min(C + c_face, 255)
//
// The mesh is a cube with a four-triangle pyramid on each side. Opposite
// pairs on a pyramid have opposite winding, so one pair contributes 1
// from the front and the other contributes 2 (and 4 from the back). The
// intended overlaps therefore land on palette entries 1, 3, 4 and 6 —
// the crossed Glenz look. A zero back color makes group 1 invisible
// from behind. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define ROTATION_SPEED 2 // radians a second, about each axis

void DEMO_Render(double deltatime)
{
	// Rotate
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_GLENZ);
}

void DEMO_Initialize(void)
{
	// Glenz polygons add palette indices instead of blending RGB values. The two
	// face groups contribute 1 or 2 when front-facing; from the back they
	// contribute 0 or 4. The intended overlaps therefore select palette entries
	// 1, 3, 4 and 6. The unused entries between them are assigned matching colors
	// so the complete additive range from 0 through 7 has a defined palette.
	RETRO_SetColor(0, RETRO_BLACK);      // background
	RETRO_SetColor(1, RETRO_MEDIUMRED);  // 1: front group 1
	RETRO_SetColor(2, RETRO_RED);        // same color as 3
	RETRO_SetColor(3, RETRO_RED);        // 1 + 2: two front-facing groups
	RETRO_SetColor(4, RETRO_LIGHTRED);   // 0 + 4: two back-facing groups
	RETRO_SetColor(5, RETRO_LIGHTRED);   // same color as 4
	RETRO_SetColor(6, RETRO_WHITE);      // 2 + 4: group 2 seen from both sides
	RETRO_SetColor(7, RETRO_WHITE);      // same color as 6

	// The object replaces each side of a cube with a four-triangle pyramid. Each
	// pyramid contains two opposite triangle pairs: one pair has outward winding
	// and contributes 1, while the other has reversed winding and contributes 2.
	// This intentional winding pattern creates the crossed Glenz appearance.
	Model3D *model = RETRO_Load3DModel("assets/glenz3.obj");
	int c[24] = { 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2 };

	// Keep the three nonzero contributions in separate bits: front-facing groups
	// add 1 or 2, and only group 2 contributes from the back, where it adds 4.
	// A zero back color makes group 1 transparent when it is back-facing.
	for (int i = 0; i < model->faces; i++) {
		model->face[i].c = c[i];
		model->face[i].backc = c[i] == 2 ? 4 : 0;
	}
}
