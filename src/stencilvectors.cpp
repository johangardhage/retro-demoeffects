//
// Stencil / metal vectors
//
// Faces as masks, not as shaded surfaces. Each visible face is filled
// with a screen-space picture whose origin is the face's own top-left
//
//   C(x, y) = Picture(x − min sx,  y − min sy)
//
// so the picture is locked to that face and slides as the face moves.
// The picture is a stack of metallic bars, one raised-cosine highlight
// per bar, packed as a wrapping 256×256 map. A slow scroll of that map
// is added to the face origin so the bars crawl. The bars are constant
// along a row, so it is the vertical half of that offset the eye sees;
// the map is kept two-dimensional because the lookup is, and a picture
// with any horizontal detail would then crawl without another line.
// Depth is the usual q-buffer. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 1.1f
#define METAL_SIZE 256
#define METAL_BAR 16 // rows a bar occupies
#define METAL_SCROLL 40 // texels of the map a second

static unsigned char Metal[METAL_SIZE * METAL_SIZE];

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED * 0.83f, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED * 0.61f, 2 * M_PI);

	// Calculate phase
	double scroll = fmod(time * METAL_SCROLL, METAL_SIZE);
	int iscroll = (int)scroll;

	// Draw cube
	RETRO_Get3DModel()->stencilmaporigin = { (float)iscroll, (float)(iscroll / 2) };
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_STENCIL);
}

void DEMO_Initialize(void)
{
	// Chrome bars: a raised-cosine highlight per METAL_BAR rows
	for (int y = 0; y < METAL_SIZE; y++) {
		float t = (y % METAL_BAR) / (float)METAL_BAR;
		int shade = 24 + 231 * (0.5f - 0.5f * cos(2 * M_PI * t));
		memset(Metal + y * METAL_SIZE, shade, METAL_SIZE);
	}

	RETRO_CreateGradientPalette(0, 80, RETRO_BLACK, RETRO_SIENNA);
	RETRO_CreateGradientPalette(80, 180, RETRO_SIENNA, RETRO_GOLD);
	RETRO_CreateGradientPalette(180, RETRO_COLORS, RETRO_GOLD, RETRO_WHITE);

	Model3D *model = RETRO_Load3DModel("assets/cubequads.obj");
	model->stencilmap = Metal;
	model->stencilmapwidth = METAL_SIZE;
	model->stencilmapheight = METAL_SIZE;
}
