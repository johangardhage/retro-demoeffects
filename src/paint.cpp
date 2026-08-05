//
// Paint
//
// A persistent framebuffer (DEMO_Render2, so nothing is cleared). The
// mouse is a single-pixel brush: left writes 255, right writes 0.
// Startup is absolute (the logical point is the window mapping). The
// relative branch, if enabled, moves by 0.2 · (dx, dy) so a fast flick
// does not jump the brush; the point is a float so those fractions
// accumulate. Leaving the 320×240 box clamps the point to the last
// pixel and warps the OS cursor back.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromouse.h"
#include "lib/retrocolor.h"

#define MOUSE_ACCEL 0.2 // relative-mode step per window pixel

void DEMO_Render2(double deltatime)
{
	static float x = RETRO_WIDTH / 2, y = RETRO_HEIGHT / 2;

	RETRO_MouseState mouse = RETRO_GetMouseState2();

	if (mouse.isrelative) {
		x += mouse.xrel * MOUSE_ACCEL;
		y += mouse.yrel * MOUSE_ACCEL;
	} else {
		x = mouse.x;
		y = mouse.y;
	}

	// Trap mouse cursor
	if (x < 0 || x > RETRO_WIDTH - 1 || y < 0 || y > RETRO_HEIGHT - 1) {
		x = CLAMPWIDTH(x);
		y = CLAMPHEIGHT(y);

		// Transform logical mouse position to window position and move mouse
		float realx, realy;
		SDL_RenderCoordinatesToWindow(RETRO.renderer, x, y, &realx, &realy);
		SDL_WarpMouseInWindow(RETRO.window, realx, realy);
	}

	// Put pixel
	if (mouse.leftbutton) {
		RETRO_PutPixel(x, y, 255);
	} else if (mouse.rightbutton) {
		RETRO_PutPixel(x, y, 0);
	}

	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(255, RETRO_WHITE);

	// Set relative mouse mode
	RETRO_SetMouseMode(false);

	// Move mouse cursor to middle of screen
	float realx, realy;
	SDL_RenderCoordinatesToWindow(RETRO.renderer, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, &realx, &realy);
	SDL_WarpMouseInWindow(RETRO.window, realx, realy);
}
