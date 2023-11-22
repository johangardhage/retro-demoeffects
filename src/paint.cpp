//
// paint.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromouse.h"

void DEMO_Render2(double deltatime)
{
	static int x = RETRO_WIDTH / 2, y = RETRO_HEIGHT / 2;
	float acceleration = 0.2;

	RETRO_MouseState mouse = RETRO_GetMouseState2();

	if (mouse.isrelative) {
		x += mouse.xrel * acceleration;
		y += mouse.yrel * acceleration;
	} else {
		x = mouse.x;
		y = mouse.y;
	}

	// Trap mouse cursor
	if (x < 0 || x > (RETRO_WIDTH - 1) || y < 0 || y > (RETRO_HEIGHT - 1)) {
		x = CLAMPWIDTH(x);
		y = CLAMPHEIGHT(y);

		// Transform logical mouse position to window position and move mouse
		int realx, realy;
		SDL_RenderLogicalToWindow(RETRO.renderer, x, y, &realx, &realy);
		SDL_WarpMouseInWindow(RETRO.window, realx, realy);
	}

	// Put pixel
	if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
		if (mouse.leftbutton) {
			RETRO_PutPixel(x, y, 255);
		} else if (mouse.rightbutton) {
			RETRO_PutPixel(x, y, 0);
		}
	}

	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, 0, 0, 0);
	RETRO_SetColor(255, 255, 255, 255);

	// Set relative mouse mode
	RETRO_SetMouseMode(false);

	// Move mouse cursor to middle of screen
	int realx, realy;
	SDL_RenderLogicalToWindow(RETRO.renderer, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, &realx, &realy);
	SDL_WarpMouseInWindow(RETRO.window, realx, realy);
}
