//
// paint.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

void DEMO_Render2(double deltatime)
{
	static bool leftMouseButtonDown = false;
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_MOUSEBUTTONUP:
			if (event.button.button == SDL_BUTTON_LEFT) {
				leftMouseButtonDown = false;
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == SDL_BUTTON_LEFT) {
				leftMouseButtonDown = true;
			}
			break;
		case SDL_MOUSEMOTION:
			if (leftMouseButtonDown) {
				int x = event.motion.x;
				int y = event.motion.y;
				RETRO_PutPixel(x, y, 255);
			}
			break;
		}
	}
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Show the cursor
	SDL_ShowCursor(1);

	// Init palette
	RETRO_SetColor(0, 0, 0, 0);
	RETRO_SetColor(255, 255, 255, 255);
}
