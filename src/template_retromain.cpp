//
// Template (retromain)
//
// Same accumulating random triangles as template_main.cpp, but through
// the library loop. DEMO_Render2 is handed the color buffer as the
// previous frame left it and flips itself. Depth is reset each stamp.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retrocolor.h"

void DEMO_Render2(double deltatime)
{
	// Draw polygons
	PolygonPoint point[3];
	point[0].x = RANDOM(RETRO_WIDTH);
	point[0].y = RANDOM(RETRO_HEIGHT);
	point[0].q = 1.0f;
	point[1].x = RANDOM(RETRO_WIDTH);
	point[1].y = RANDOM(RETRO_HEIGHT);
	point[1].q = 1.0f;
	point[2].x = RANDOM(RETRO_WIDTH);
	point[2].y = RANDOM(RETRO_HEIGHT);
	point[2].q = 1.0f;

	// Depth is cleared so the new triangle can overwrite; color is not.
	RETRO_ClearDepthBuffer();
	RETRO_DrawFlatPolygon(point, 3, RANDOM(RETRO_COLORS));
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, RETRO_BLACK);
	for (int i = 1; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS));
	}
}
