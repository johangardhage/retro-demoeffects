//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETRO_H_
#define _RETRO_H_

#include <SDL3/SDL.h>
#include <getopt.h> // getopt_long
#include <libgen.h> // basename
#include <limits.h> // INT_MIN
#include <math.h> // cos, sin, pow
#include <stdio.h> // FILE
#include <stdlib.h> // atoi, exit, free, malloc, rand, srand
#include <string.h> // memcpy, memset
#include <time.h> // time

// *******************************************************************
// Public dynamic functions
// *******************************************************************

void __attribute__((weak)) DEMO_Startup(void);
void __attribute__((weak)) DEMO_Initialize(void);
void __attribute__((weak)) DEMO_Deinitialize(void);
void __attribute__((weak)) DEMO_Render(double deltatime);
void __attribute__((weak)) DEMO_Render2(double deltatime);

// *******************************************************************
// Private dynamic functions
// *******************************************************************

void __attribute__((weak)) RETRO_Initialize_3D(void);
void __attribute__((weak)) RETRO_Deinitialize_3D(void);

// *******************************************************************
// Public variables
// *******************************************************************

#ifndef RETRO_WIDTH
#define RETRO_WIDTH 320
#endif
#ifndef RETRO_HEIGHT
#define RETRO_HEIGHT 240
#endif
#ifndef RETRO_SIMULATION_STEP
#define RETRO_SIMULATION_STEP (1.0 / 60.0)
#endif
#ifndef RETRO_MAX_SIMULATION_STEPS
#define RETRO_MAX_SIMULATION_STEPS 15
#endif

#define RETRO_COLORS 256

#define RETRO_MAX_IMAGES 10

#define RETRO_SINCOS_ANGLE 256

#define RAD2DEG (180 / M_PI)
#define DEG2RAD (M_PI / 180)
#define RAND ((double)rand() / ((double)RAND_MAX + 1))
#define RANDOM(n) ((int)(RAND * (n)))
#define RANDOMF(n) ((float)(RAND * (n)))
#define COS(x) cos(((x) * 2.0 * M_PI) / RETRO_SINCOS_ANGLE)
#define SIN(x) sin(((x) * 2.0 * M_PI) / RETRO_SINCOS_ANGLE)
#define CLAMP(n, l, h) ((n) < (l) ? (l) : ((n) > ((h) - 1) ? ((h) - 1) : (int)(n)))
#define CLAMP64(n) ((n) < 0 ? 0 : ((n) > 63 ? 63 : (int)(n)))
#define CLAMP128(n) ((n) < 0 ? 0 : ((n) > 127 ? 127 : (int)(n)))
#define CLAMP256(n) ((n) < 0 ? 0 : ((n) > 255 ? 255 : (int)(n)))
#define CLAMPWIDTH(n) ((n) < 0 ? 0 : ((n) > RETRO_WIDTH - 1 ? RETRO_WIDTH - 1 : (int)(n)))
#define CLAMPHEIGHT(n) ((n) < 0 ? 0 : ((n) > RETRO_HEIGHT - 1 ? RETRO_HEIGHT - 1 : (int)(n)))
#define WRAP(n, h) (((int)(n) % (h) + (h)) % (h))
#define WRAP64(n) ((int)(n) & 63)
#define WRAP128(n) ((int)(n) & 127)
#define WRAP256(n) ((int)(n) & 255)
#define WRAPWIDTH(n) (((int)(n) % RETRO_WIDTH + RETRO_WIDTH) % RETRO_WIDTH)
#define WRAPHEIGHT(n) (((int)(n) % RETRO_HEIGHT + RETRO_HEIGHT) % RETRO_HEIGHT)
#define SWAP(x, y) do { typeof(x) _SWAP = x; x = y; y = _SWAP; } while (0)
#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#define MAX(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

struct RETRO_Palette {
	unsigned char r, g, b;
};

struct RETRO_Image {
	RETRO_Palette palette[256];
	unsigned char *data;
	int width;
	int height;
};

// *******************************************************************
// Private variables
// *******************************************************************

enum { RETRO_MODE_FULLSCREEN, RETRO_MODE_FULLWINDOW, RETRO_MODE_WINDOW };

struct {
	int mode;
	char *basename;
	bool stretch;
	bool vsync;
	bool linear;
	bool showcursor;
	bool showfps;
	int fpscap;
	bool quit;
	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	SDL_Texture *renderbuffer = NULL;
	unsigned char *framebuffer = NULL;
	int framebuffersize;
	unsigned int palette[RETRO_COLORS];
	RETRO_Image *image[RETRO_MAX_IMAGES];
	int images = 0;
	const bool *keystate;
	bool keydown[256];
	int yoffset[RETRO_HEIGHT];
	double accumulator = 0;
} RETRO = { .mode = RETRO_MODE_FULLSCREEN, .stretch = false, .vsync = true, .showfps = true };

// *******************************************************************
// Public functions
// *******************************************************************

void RETRO_RageQuit(const char *message, const char *error = "")
{
	printf(message, error);
	exit(-1);
}

unsigned char *RETRO_FrameBuffer(void)
{
	return RETRO.framebuffer;
}

RETRO_Palette RETRO_GetColor(int color)
{
	RETRO_Palette palette;
	palette.r = (RETRO.palette[color] >> 16) & 0xff;
	palette.g = (RETRO.palette[color] >> 8) & 0xff;
	palette.b = RETRO.palette[color] & 0xff;
	return palette;
}

RETRO_Palette RETRO_Get6bitColor(int color)
{
	RETRO_Palette palette;
	palette.r = ((RETRO.palette[color] >> 16) & 0xff) >> 2;
	palette.g = ((RETRO.palette[color] >> 8) & 0xff) >> 2;
	palette.b = ((RETRO.palette[color]) & 0xff) >> 2;
	return palette;
}

void RETRO_SetColor(int color, unsigned char r, unsigned char g, unsigned char b)
{
	RETRO.palette[color] = 0xff000000 | (r << 16) | (g << 8) | (b);
}

//
// Set a color in a palette buffer, or in the active palette when no buffer is
// given. Writing to a buffer copies the components as they are given, which
// allows both 6-bit and 8-bit palettes
//
void RETRO_SetColor(int index, RETRO_Palette color, RETRO_Palette *buffer = NULL)
{
	if (buffer) {
		buffer[index] = color;
	} else {
		RETRO_SetColor(index, color.r, color.g, color.b);
	}
}

void RETRO_Set6bitColor(int color, unsigned char r, unsigned char g, unsigned char b)
{
	r = (r & 63) << 2;
	g = (g & 63) << 2;
	b = (b & 63) << 2;
	RETRO.palette[color] = 0xff000000 | (r << 16) | (g << 8) | (b);
}

void RETRO_SetPalette(RETRO_Palette *palette, int colors = RETRO_COLORS)
{
	for (int i = 0; i < colors; i++) {
		RETRO_SetColor(i, palette[i].r, palette[i].g, palette[i].b);
	}
}

void RETRO_Set6bitPalette(RETRO_Palette *palette, int colors = RETRO_COLORS)
{
	for (int i = 0; i < colors; i++) {
		RETRO_Set6bitColor(i, palette[i].r, palette[i].g, palette[i].b);
	}
}

void RETRO_PutPixel(int x, int y, unsigned char color)
{
	RETRO.framebuffer[RETRO.yoffset[y] + x] = color;
}

unsigned char RETRO_GetPixel(int x, int y)
{
	return RETRO.framebuffer[RETRO.yoffset[y] + x];
}

void RETRO_Clear(unsigned char color = 0, int size = RETRO.framebuffersize, unsigned char *dest = RETRO.framebuffer)
{
	memset(dest, color, size);
}

void RETRO_Blit(unsigned char *src, int size = RETRO.framebuffersize, unsigned char *dest = RETRO.framebuffer)
{
	memcpy(dest, src, size);
}

int *RETRO_Yoffset(void)
{
	return RETRO.yoffset;
}

unsigned char *RETRO_ImageData(int id = 0)
{
	return RETRO.image[id] ? RETRO.image[id]->data : NULL;
}

RETRO_Palette *RETRO_ImagePalette(int id = 0)
{
	return RETRO.image[id] ? RETRO.image[id]->palette : NULL;
}

RETRO_Image *RETRO_AllocateImage(void)
{
	RETRO.image[RETRO.images] = (RETRO_Image *)malloc(sizeof(RETRO_Image));
	if (RETRO.image[RETRO.images] == NULL) {
		RETRO_RageQuit("Cannot allocate image memory\n");
	}
	return RETRO.image[RETRO.images++];
}

void RETRO_FreeImage(int id = 0)
{
	if (RETRO.image[id]) {
		if (RETRO.image[id]->data) {
			free(RETRO.image[id]->data);
			RETRO.image[id]->data = NULL;
		}
		free(RETRO.image[id]);
		RETRO.image[id] = NULL;
		RETRO.images--;
	}
}

RETRO_Image *RETRO_LoadImage(const char *filename, bool setpalette = false)
{
	RETRO_Image *image = RETRO_AllocateImage();

	// Open file
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	// Read header
	unsigned char header[128];
	if (fread(header, 128, 1, fp) != 1 || header[0] != 10) {
		RETRO_RageQuit("Cannot read file: %s\n", filename);
	}

	// From header data, build some image info
	int xmin = (header[4] + (header[5] << 8));
	int ymin = (header[6] + (header[7] << 8));
	int xmax = (header[8] + (header[9] << 8));
	int ymax = (header[10] + (header[11] << 8));

	// Calculate the size of image
	image->width = xmax - xmin + 1;
	image->height = ymax - ymin + 1;

	// Reserve memory
	image->data = (unsigned char *)malloc(image->width * image->height);
	if (image->data == NULL) {
		RETRO_RageQuit("Cannot allocate image data memory\n");
	}

	// Unpack image
	int size = image->width * image->height;
	int index = 0;
	while (index < size) {
		int data = getc(fp);
		if (data == EOF) {
			RETRO_RageQuit("Cannot read file: %s\n", filename);
		}
		if (data < 192) {
			image->data[index++] = data;
		} else {
			int num = data - 192;
			data = getc(fp);
			if (data == EOF) {
				RETRO_RageQuit("Cannot read file: %s\n", filename);
			}
			while (num-- > 0 && index < size) {
				image->data[index++] = data;
			}
		}
	}

	// Read palette from end of file
	fseek(fp, -768, SEEK_END);
	for (int i = 0; i < 256; i++) {
		image->palette[i].r = fgetc(fp);
		image->palette[i].g = fgetc(fp);
		image->palette[i].b = fgetc(fp);
	}

	// Close file
	fclose(fp);

	// Set palette
	if (setpalette) {
		RETRO_SetPalette(image->palette);
	}

	return image;
}

void RETRO_Flip(void)
{
	// Copy framebuffer
	unsigned int *pixels;
	int pitch;
	SDL_LockTexture(RETRO.renderbuffer, NULL, (void **)&pixels, &pitch);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		unsigned int *row = (unsigned int *)((unsigned char *)pixels + y * pitch);
		for (int x = 0; x < RETRO_WIDTH; x++) {
			row[x] = RETRO.palette[RETRO.framebuffer[RETRO.yoffset[y] + x]];
		}
	}
	SDL_UnlockTexture(RETRO.renderbuffer);

	SDL_RenderClear(RETRO.renderer);
	SDL_RenderTexture(RETRO.renderer, RETRO.renderbuffer, NULL, NULL);
	SDL_RenderPresent(RETRO.renderer);
}

void RETRO_Initialize(void)
{
	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		RETRO_RageQuit("SDL_Init failed: %s\n", SDL_GetError());
	}

	// Get current display mode
	SDL_DisplayID display = SDL_GetPrimaryDisplay();
	if (display == 0) {
		RETRO_RageQuit("SDL_GetPrimaryDisplay failed: %s\n", SDL_GetError());
	}
	const SDL_DisplayMode *dm = SDL_GetCurrentDisplayMode(display);
	if (dm == NULL) {
		RETRO_RageQuit("SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
	}

	// Set size of window
	int window_width = dm->w;
	int window_height = dm->h;
	if (RETRO.mode == RETRO_MODE_WINDOW) {
		window_width = RETRO_WIDTH;
		window_height = RETRO_HEIGHT;
	}
	SDL_WindowFlags window_flags = 0;
	if (RETRO.mode == RETRO_MODE_FULLWINDOW) {
		window_flags |= SDL_WINDOW_BORDERLESS;
	}

	// Create window title
	char title[128];
	snprintf(title, 128, "RETRO - %s", RETRO.basename);

	// Create window
	RETRO.window = SDL_CreateWindow(title, window_width, window_height, window_flags);
	if (RETRO.window == NULL) {
		RETRO_RageQuit("SDL_CreateWindow failed: %s\n", SDL_GetError());
	}

	// Create renderer
	RETRO.renderer = SDL_CreateRenderer(RETRO.window, NULL);
	if (RETRO.renderer == NULL) {
		RETRO_RageQuit("SDL_CreateRenderer failed: %s\n", SDL_GetError());
	}
	SDL_SetRenderVSync(RETRO.renderer, RETRO.vsync ? 1 : SDL_RENDERER_VSYNC_DISABLED);

	// Stretch screen
	if (RETRO.stretch == false) {
		SDL_SetRenderLogicalPresentation(RETRO.renderer, RETRO_WIDTH, RETRO_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	}

	// Set fullscreen
	if (RETRO.mode == RETRO_MODE_FULLSCREEN) {
		if (!SDL_SetWindowFullscreenMode(RETRO.window, dm)) {
			RETRO_RageQuit("SDL_SetWindowFullscreenMode failed: %s\n", SDL_GetError());
		}
		if (!SDL_SetWindowFullscreen(RETRO.window, true)) {
			RETRO_RageQuit("SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
		}
		SDL_SyncWindow(RETRO.window);
	}

	// Create render buffer
	RETRO.renderbuffer = SDL_CreateTexture(RETRO.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RETRO_WIDTH, RETRO_HEIGHT);
	if (RETRO.renderbuffer == NULL) {
		RETRO_RageQuit("SDL_CreateTexture failed: %s\n", SDL_GetError());
	}
	SDL_SetTextureBlendMode(RETRO.renderbuffer, SDL_BLENDMODE_NONE);
	SDL_SetTextureScaleMode(RETRO.renderbuffer, RETRO.linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);

	// Create framebuffer
	RETRO.framebuffersize = RETRO_WIDTH * RETRO_HEIGHT;
	RETRO.framebuffer = (unsigned char *)malloc(RETRO.framebuffersize);
	if (RETRO.framebuffer == NULL) {
		RETRO_RageQuit("Cannot allocate framebuffer memory\n");
	}
	memset(RETRO.framebuffer, 0, RETRO.framebuffersize);

	// Cursor
	if (RETRO.showcursor) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}

	// Build Y offset table
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		RETRO.yoffset[y] = y * RETRO_WIDTH;
	}

	// Initialize random number generator
	srand(time(NULL));

	if (RETRO_Initialize_3D) RETRO_Initialize_3D();
}

void RETRO_Deinitialize(void)
{
	if (RETRO_Deinitialize_3D) RETRO_Deinitialize_3D();

	for (int i = 0; i < RETRO_MAX_IMAGES; i++) {
		RETRO_FreeImage(i);
	}

	if (RETRO.framebuffer) {
		free(RETRO.framebuffer);
	}

	SDL_DestroyTexture(RETRO.renderbuffer);
	SDL_DestroyRenderer(RETRO.renderer);
	SDL_DestroyWindow(RETRO.window);
	SDL_Quit();
}

void RETRO_SetVSync(bool state = true)
{
	SDL_SetRenderVSync(RETRO.renderer, state ? 1 : SDL_RENDERER_VSYNC_DISABLED);
	RETRO.vsync = state;
}

double RETRO_DeltaTime(void)
{
	static unsigned long int now = SDL_GetPerformanceCounter();
	static unsigned long int old = 0;

	old = now;
	now = SDL_GetPerformanceCounter();

	return (double)(now - old) / SDL_GetPerformanceFrequency();
}

//
// Consumes one fixed simulation step, to be used as the condition of a while loop:
//
//   while (RETRO_PerformSimulation()) {
//       // advance the effect by exactly RETRO_SIMULATION_STEP seconds
//   }
//
// The mainloop hands each frame's elapsed time to the accumulator, and every call here
// takes one fixed step back out of it until less than a whole step is left. Stepping at a
// fixed rate rather than once per frame keeps an effect running at the same speed whatever
// the display does, and lets effects that advance in discrete units - integer positions,
// table indices, palette entries - stay exact instead of accumulating rounding error.
//
// Time left over stays in the accumulator and carries into the next frame, so the
// simulation keeps up with the wall clock without drifting. The accumulator is capped at
// RETRO_MAX_SIMULATION_STEPS so a long stall cannot demand an unbounded burst of catch-up
// steps, which would only make the next frame later still. Past that limit the simulation
// runs slow instead.
//
bool RETRO_PerformSimulation(void)
{
	if (RETRO.accumulator < RETRO_SIMULATION_STEP) {
		return false;
	}

	RETRO.accumulator -= RETRO_SIMULATION_STEP;

	return true;
}

bool RETRO_KeyState(SDL_Scancode key)
{
	return RETRO.keystate[key];
}

bool RETRO_KeyPressed(SDL_Scancode key)
{
	if (key > 255) return false;
	if (RETRO.keystate[key]) {
		if (RETRO.keydown[key]) {
			return false;
		} else {
			RETRO.keydown[key] = true;
			return true;
		}
	}
	RETRO.keydown[key] = false;
	return false;
}

void RETRO_Quit(void)
{
	RETRO.quit = true;
}

bool RETRO_QuitRequested(void)
{
	SDL_PumpEvents();
	RETRO.keystate = SDL_GetKeyboardState(NULL);
	if (SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0) {
		return true;
	} else if (RETRO.quit) {
		return true;
	} else if (RETRO.keystate[SDL_SCANCODE_ESCAPE]) {
		return true;
	} else if (RETRO.keystate[SDL_SCANCODE_Q]) {
		return true;
	}
	return false;
}

// *******************************************************************
// Private functions
// *******************************************************************

void RETRO_Mainloop(void)
{
	while (!RETRO_QuitRequested()) {
		double deltatime = RETRO_DeltaTime();

		// Check events
		if (RETRO.keystate[SDL_SCANCODE_SPACE]) {
			continue;
		}

		// Feed the fixed step accumulator that RETRO_PerformSimulation drains. Done after
		// the pause check, so time spent paused is discarded rather than caught up on.
		RETRO.accumulator = MIN(RETRO.accumulator + deltatime, RETRO_SIMULATION_STEP * RETRO_MAX_SIMULATION_STEPS);

		// Render scene
		unsigned long int start = SDL_GetTicks();
		if (DEMO_Render) {
			RETRO_Clear();
			DEMO_Render(deltatime);
			RETRO_Flip();
		} else if (DEMO_Render2) {
			DEMO_Render2(deltatime);
		}
		unsigned long int stop = SDL_GetTicks();

		// Limit FPS
		if (RETRO.fpscap && ((stop - start) < 1000UL / RETRO.fpscap)) {
			SDL_Delay((1000 / RETRO.fpscap) - (stop - start));
		}

		// Show FPS once a second
		if (RETRO.showfps) {
			static unsigned long int fpsticks = SDL_GetTicks();
			static int fpscount = 0;
			if (fpsticks < SDL_GetTicks() - 1000UL) {
				char title[128];
				snprintf(title, 128, "RETRO - %s - FPS: %d", RETRO.basename, fpscount);
				SDL_SetWindowTitle(RETRO.window, title);
				fpsticks = SDL_GetTicks();
				fpscount = 0;
			}
			fpscount++;
		}
	}
}

#endif
