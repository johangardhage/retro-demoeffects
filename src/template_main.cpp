//
// Template (main)
//
// The raw entry point: own main(), RETRO_Initialize / Flip /
// Deinitialize, no DEMO_* callbacks. Each frame stamps a random
// 3-vertex flat triangle. Color is never cleared, so the triangles
// accumulate; the depth buffer is reset each stamp so the new
// triangle can overwrite. SPACE pauses the loop the same way the
// library's own main loop does.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retropoly.h"
#include "lib/retropalette.h"

int main(int argc, char *argv[])
{
	RETRO_Initialize();

	// Init palette
	RETRO_SetColor(0, RETRO_BLACK);
	for (int i = 1; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS));
	}

	while (!RETRO_QuitRequested()) {
		// Check events
		if (RETRO_KeyState(SDL_SCANCODE_SPACE)) {
			continue;
		}

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

	RETRO_Deinitialize();

	return 0;
}
