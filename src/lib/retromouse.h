//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMOUSE_H_
#define _RETROMOUSE_H_

#include "retro.h"

// *******************************************************************
// Private variables
// *******************************************************************

struct RETRO_MouseState {
	bool isrelative;
	int x, y;
	int xrel, yrel;
	bool leftbutton, rightbutton;
	unsigned int leftcount, rightcount;
};

struct {
	RETRO_MouseState state;
} RETRO_Mouse;

// *******************************************************************
// Public functions
// *******************************************************************

RETRO_MouseState RETRO_GetMouseState(void)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			if (event.button.button == SDL_BUTTON_LEFT) {
				RETRO_Mouse.state.leftbutton = false;
			} else if (event.button.button == SDL_BUTTON_RIGHT) {
				RETRO_Mouse.state.rightbutton = false;
			}
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			if (event.button.button == SDL_BUTTON_LEFT) {
				RETRO_Mouse.state.leftbutton = true;
			} else if (event.button.button == SDL_BUTTON_RIGHT) {
				RETRO_Mouse.state.rightbutton = true;
			}
		} else if (event.type == SDL_EVENT_MOUSE_MOTION) {
			RETRO_Mouse.state.x = event.motion.x;
			RETRO_Mouse.state.y = event.motion.y;
			RETRO_Mouse.state.xrel = event.motion.xrel;
			RETRO_Mouse.state.yrel = event.motion.yrel;
		}
	}
	RETRO_Mouse.state.isrelative = SDL_GetWindowRelativeMouseMode(RETRO.window);

	// Increase counter if buttons are clicked
	RETRO_Mouse.state.leftcount = (RETRO_Mouse.state.leftbutton ? RETRO_Mouse.state.leftcount + 1 : 0);
	RETRO_Mouse.state.rightcount = (RETRO_Mouse.state.rightbutton ? RETRO_Mouse.state.rightcount + 1 : 0);

	return RETRO_Mouse.state;
}

RETRO_MouseState RETRO_GetMouseState2(void)
{
	// Get mouse window position
	float x1, y1;
	SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x1, &y1);

	// Transform window position to render area (logical) position
	float x2, y2;
	SDL_RenderCoordinatesFromWindow(RETRO.renderer, x1, y1, &x2, &y2);

	// Get relative mouse position
	float xrel, yrel;
	SDL_GetRelativeMouseState(&xrel, &yrel);

	// Set mouse state
	RETRO_Mouse.state.x = x2;
	RETRO_Mouse.state.y = y2;
	RETRO_Mouse.state.xrel = xrel;
	RETRO_Mouse.state.yrel = yrel;
	RETRO_Mouse.state.leftbutton = buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT);
	RETRO_Mouse.state.rightbutton = buttons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT);
	RETRO_Mouse.state.isrelative = SDL_GetWindowRelativeMouseMode(RETRO.window);

	// Increase counter if buttons are clicked
	RETRO_Mouse.state.leftcount = (RETRO_Mouse.state.leftbutton ? RETRO_Mouse.state.leftcount + 1 : 0);
	RETRO_Mouse.state.rightcount = (RETRO_Mouse.state.rightbutton ? RETRO_Mouse.state.rightcount + 1 : 0);

	return RETRO_Mouse.state;
}

void RETRO_SetMouseMode(bool relative, bool cursor = false)
{
	SDL_SetWindowRelativeMouseMode(RETRO.window, relative);
	if (cursor) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
}

#endif
