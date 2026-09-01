//
// Vector logo
//
// The word RETRO as a solid, one Lambert term per face: the shading of
// flatshadedcube.cpp on a mesh that is not a cube. assets/retrologo.obj is
// twenty convex strokes extruded along z, because a letter is concave and the
// polygon drawer fans from the first vertex, which only fills a convex outline.
//
// The strokes tile a letter's face rather than overlapping it, so its front is
// several coplanar quads that meet along shared edges and read as one surface.
// What the strokes do not carry is the walls between them: a wall two strokes
// stand against is inside the solid, and the asset holds only the walls on the
// boundary of their union. A buried wall costs more than the fill it wastes,
// because a wall drawn at a grazing angle spills a pixel past its own edge, and
// one buried in the middle of a letter spills that pixel over the lit face in
// front of it.
//
// The letters stand up unrotated because the model's y is the screen's, growing
// down, and their front faces the eye at -z, which is where the light shines
// from. The word is at its brightest facing the viewer and darkens as it turns,
// which is the one pose a logo is really for.
//
// The turn is a full spin about y with a slow elliptic rock about x and z:
//
//   ax = ROCK sin(phase),  az = ROCK cos(phase)
//
// and the word is carried LOGO_DISTANCE back first. The word is six units wide
// and barely one deep, so it reaches furthest forward turned side-on, a quarter
// turn from either face: the near end comes to two fifths of the eye distance
// without the shift, and perspective swells that end until one letter's side
// wall fills the screen. Half a turn is the safe pose, not the dangerous one.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define SPIN_SPEED 1.1 // radians a second about y
#define ROCK_SPEED 0.7 // radians a second around the rock
#define ROCK 0.25 // radians the rock reaches about x and about z
#define LOGO_DISTANCE 2.0 // model units the word stands behind the origin

void DEMO_Render(double deltatime)
{
	// Rotate
	static float ay;
	static double phase = 0;
	ay = fmod(ay + deltatime * SPIN_SPEED, 2 * M_PI);
	phase = fmod(phase + deltatime * ROCK_SPEED, 2 * M_PI);
	float ax = ROCK * sin(phase);
	float az = ROCK * cos(phase);

	// Draw logo
	RETRO_RotateModel(ax, ay, az);
	RETRO_TranslateModel(0, 0, LOGO_DISTANCE);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	// Init palette. Matte, because a flat lit face has one normal for all of
	// it and a specular highlight would flash the whole face at once
	RETRO_CreateMattePalette(RETRO_GOLD);

	Model3D *model = RETRO_Load3DModel("assets/retrologo.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;

	RETRO_InitializeLightSource(0, 0, -1);
}
