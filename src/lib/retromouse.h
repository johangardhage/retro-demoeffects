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

inline struct {
	RETRO_MouseState state;
	bool discardmotion;
} RETRO_Mouse;

// *******************************************************************
// Public functions
// *******************************************************************

inline RETRO_MouseState RETRO_GetMouseState(void)
{
	// Polled via SDL_Get*MouseState rather than SDL_PollEvent: the event queue is
	// drained solely by RETRO_QuitRequested, and a second poller here would steal
	// quit/key events out from under it. SDL_GetRelativeMouseState keeps its own
	// accumulator independent of the event queue, so it stays exact regardless.
	//
	// Get mouse window position
	float x1, y1;
	SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x1, &y1);

	// Transform window position to render area (logical) position
	float x2, y2;
	SDL_RenderCoordinatesFromWindow(RETRO.renderer, x1, y1, &x2, &y2);

	// Get relative mouse position
	float xrel, yrel;
	SDL_GetRelativeMouseState(&xrel, &yrel);

	// Swallow the transition jump from a relative-mode grab: wait for the
	// first nonzero delta (the grab itself), discard it, then resume.
	if (RETRO_Mouse.discardmotion) {
		if (xrel != 0.0f || yrel != 0.0f) {
			RETRO_Mouse.discardmotion = false;
		}
		xrel = 0.0f;
		yrel = 0.0f;
	}

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

inline void RETRO_SetMouseMode(bool relative, bool cursor = false)
{
	if (relative && !SDL_GetWindowRelativeMouseMode(RETRO.window)) {
		RETRO_Mouse.discardmotion = true;
	}
	SDL_SetWindowRelativeMouseMode(RETRO.window, relative);
	if (cursor) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
}

#endif
