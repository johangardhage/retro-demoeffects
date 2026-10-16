//
// Bump glow
//
// A torus turning in place, lit by a glow map, white through red into yellow,
// read by its view-space normal (RETRO_POLY_MATCAP), with a bump that pulses
// across its surface.
//
// The glow map, bumpglow_glowmap_128x128.pcx, is centered at 64 and reaches
// MAP_RADIUS texels out. The mesh, flowertorus.obj, is in the library's
// axes and sized for its default projection, and its UVs tile the height
// field four times around.
//
// The bump is a height field, bumpheight_256x256.pcx, whose gradient tilts
// the normal along the surface, so the glow slides over the relief as the torus
// turns. Its strength rises and falls as the first half of a sine, once every
// 1.26 seconds, and sets the bump's grazing height: the lower the height, the
// steeper the tilt.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromodel.h"
#include "lib/retrorender.h"

constexpr int MAP_RADIUS = 54;             // the glow map's reach from its middle
constexpr double GRAZING = 195.8;          // the bump's grazing height at its strongest
constexpr double PULSE_SPEED = 2.5;        // radians a second, half a turn a rise and fall
constexpr double ROTATION_SPEED = 0.5;     // radians a second, about each axis

void DEMO_Render(RETRO_Time time)
{
	// The grazing height is GRAZING at the bump's strongest, and grows as the
	// pulse fades, which flattens the tilt. Held at 1/383 of the strongest,
	// the tilt is too small to see.
	Model3D *model = RETRO_Get3DModel();
	float pulse = sin(fmod(time.total * PULSE_SPEED, M_PI));
	model->bumpgrazing = (int)(GRAZING / MAX(pulse, 1.0f / 383) + 0.5f);

	// The same turn about x, y and z
	float angle = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	RETRO_RotateModel(angle, angle, angle);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_MATCAP);
}

void DEMO_Initialize(void)
{
	RETRO_Image *glow = RETRO_LoadImage("assets/bumpglow_glowmap_128x128.pcx", true);
	Model3D *model = RETRO_Load3DModel("assets/flowertorus.obj");
	model->bumpmap = RETRO_LoadImage("assets/bumpheight_256x256.pcx")->data;
	model->envmap = glow->data;
	model->envmapwidth = glow->width;
	model->envmapheight = glow->height;
	model->envmapradius = MAP_RADIUS;
}
