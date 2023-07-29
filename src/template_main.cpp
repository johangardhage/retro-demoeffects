//
// template_main.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retropoly.h"

int main(int argc, char *argv[])
{
	RETRO_Initialize();

	// Init palette
	RETRO_SetColor(0, 0, 0, 0);
	for (int i = 1; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS), RANDOM(RETRO_COLORS));
	}

	while (true) {
		// Check events
		SDL_PumpEvents();
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		if (SDL_QuitRequested()) {
			break;
		}
		if (keys[SDL_GetScancodeFromKey(SDLK_ESCAPE)]) {
			break;
		}
		if (keys[SDL_GetScancodeFromKey(SDLK_q)]) {
			break;
		}

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

	RETRO_Deinitialize();

	return 0;
}
