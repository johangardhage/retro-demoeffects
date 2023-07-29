//
// template_retromain.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"

void DEMO_Render2(double deltatime)
{
	// Draw polygons
	PolygonPoint points[3];
	points[0].x = RANDOM(RETRO_WIDTH);
	points[0].y = RANDOM(RETRO_HEIGHT);
	points[1].x = RANDOM(RETRO_WIDTH);
	points[1].y = RANDOM(RETRO_HEIGHT);
	points[2].x = RANDOM(RETRO_WIDTH);
	points[2].y = RANDOM(RETRO_HEIGHT);

	RETRO_DrawFlatPolygon(points, 3, RANDOM(RETRO_COLORS));
	RETRO_Flip();
}

void DEMO_Initialize()
{
	// Init palette
	RETRO_SetColor(0, 0, 0, 0);
	for (int i = 1; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS));
	}
}
