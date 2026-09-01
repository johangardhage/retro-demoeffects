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
#include <stdarg.h> // va_list, vprintf
#include <stdio.h> // FILE
#include <stdlib.h> // atoi, exit, free, malloc, rand, srand
#include <string.h> // memcpy, memset
#include <time.h> // time

// *******************************************************************
// Public dynamic functions
// *******************************************************************

//
// A demo defines the callbacks it needs and leaves the rest undefined
//
// DEMO_Startup runs before the screen is created, so a demo that defines nothing else never
// opens one. DEMO_Initialize and DEMO_Deinitialize bracket the mainloop, with the
// framebuffer and the palette in place.
//
// DEMO_FixedUpdate advances the effect by timestep seconds, always exactly
// RETRO_SIMULATION_STEP. The mainloop calls it once for every whole step the frame has
// earned, which can be no times at all on a fast display and several while catching up
// after a stall, so work that belongs to a single frame does not belong here. See
// RETRO_AdvanceSimulation.
//
// DEMO_Render draws a frame into a cleared framebuffer, which is shown once it returns.
// DEMO_Render2 is handed the framebuffer as the previous frame left it and shows it itself
// with RETRO_Flip. A demo that draws in DEMO_FixedUpdate needs neither, and gets its framebuffer
// shown as it stands.
//
void __attribute__((weak)) DEMO_Startup(void);
void __attribute__((weak)) DEMO_Initialize(void);
void __attribute__((weak)) DEMO_Deinitialize(void);
void __attribute__((weak)) DEMO_FixedUpdate(double timestep);
void __attribute__((weak)) DEMO_Render(double time, double deltatime);
void __attribute__((weak)) DEMO_Render2(double time, double deltatime);

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

#define RETRO_DEGREES_PER_TURN 360

#define RAD2DEG (180 / M_PI)
#define DEG2RAD (M_PI / 180)

#define SWAP(a, b) do { __typeof__(a) _swap = (a); (a) = (b); (b) = _swap; } while (0)
#define MIN(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a < _b ? _a : _b; })
#define MAX(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })

// A random number in [0, 1), and the same scaled to [0, n) as an int or a float
inline double RAND() { return (double)rand() / ((double)RAND_MAX + 1); }
inline int RANDOM(double n) { return (int)(RAND() * n); }
inline float RANDOMF(double n) { return (float)(RAND() * n); }

// Cosine and sine over RETRO_SINCOS_ANGLE units per turn rather than 2*pi radians
inline double COS(double x) { return cos((x * 2.0 * M_PI) / RETRO_SINCOS_ANGLE); }
inline double SIN(double x) { return sin((x * 2.0 * M_PI) / RETRO_SINCOS_ANGLE); }

// Clamp n into [l, h - 1], so h is one past the highest value the result can take.
//
// The integer overload keeps integer effects free of floating point. The floating point
// one truncates toward zero, which matches flooring because every lower bound in use is
// non-negative, so a value that would round the wrong way is clamped to l first.
// float uses that path. unsigned and long forward to the integer path so those call
// sites are not ambiguous.
inline int CLAMP(int n, int l, int h) { return n < l ? l : (n > h - 1 ? h - 1 : n); }
inline int CLAMP(double n, int l, int h) { return n < l ? l : (n > h - 1 ? h - 1 : (int)n); }
inline int CLAMP(float n, int l, int h) { return CLAMP((double)n, l, h); }
inline int CLAMP(unsigned n, int l, int h) { return CLAMP((int)n, l, h); }
inline int CLAMP(long n, int l, int h) { return CLAMP((int)n, l, h); }
inline int CLAMP(unsigned long n, int l, int h) { return CLAMP((int)n, l, h); }
#define CLAMP64(n) CLAMP((n), 0, 64)
#define CLAMP128(n) CLAMP((n), 0, 128)
#define CLAMP256(n) CLAMP((n), 0, 256)
#define CLAMP360(n) CLAMP((n), 0, 360)
#define CLAMPWIDTH(n) CLAMP((n), 0, RETRO_WIDTH)
#define CLAMPHEIGHT(n) CLAMP((n), 0, RETRO_HEIGHT)

// Clamp n into [0, 1], keeping the argument's own precision rather than widening it
inline float CLAMP01(float n) { return n < 0 ? 0 : (n > 1 ? 1 : n); }
inline double CLAMP01(double n) { return n < 0 ? 0 : (n > 1 ? 1 : n); }

// Wrap n into [0, h). Negative input wraps from the top, so WRAP(-1, 64) is 63.
//
// The integer overload keeps integer effects free of floating point. The floating point
// one floors before taking the remainder, so a negative fraction wraps to the same cell
// its floor does rather than to the cell above it.
// float uses that path. unsigned and long forward to the integer path so those call
// sites are not ambiguous.
inline int WRAP(int n, int h) { int r = n % h; return r < 0 ? r + h : r; }
inline int WRAP(double n, int h) { int r = (int)floor(n) % h; return r < 0 ? r + h : r; }
inline int WRAP(float n, int h) { return WRAP((double)n, h); }
inline int WRAP(unsigned n, int h) { return WRAP((int)n, h); }
inline int WRAP(long n, int h) { return WRAP((int)n, h); }
inline int WRAP(unsigned long n, int h) { return WRAP((int)n, h); }
#define WRAP64(n) WRAP((n), 64)
#define WRAP128(n) WRAP((n), 128)
#define WRAP256(n) WRAP((n), 256)
#define WRAP360(n) WRAP((n), 360)
#define WRAPWIDTH(n) WRAP((n), RETRO_WIDTH)
#define WRAPHEIGHT(n) WRAP((n), RETRO_HEIGHT)

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
	bool keylatched[256];
	int yoffset[RETRO_HEIGHT];
	double accumulator = 0;
	double time = 0;
} RETRO = { .mode = RETRO_MODE_FULLSCREEN, .stretch = false, .vsync = true, .showfps = true };

// *******************************************************************
// Public functions
// *******************************************************************

void RETRO_RageQuit(const char *message, ...)
{
	va_list args;
	va_start(args, message);
	vprintf(message, args);
	va_end(args);
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

//
// An 8-bit color on the 6-bit scale the VGA DAC works in. RETRO_Palette carries
// its components raw, so which scale one is on is the caller's to keep track
// of, and the two are not interchangeable: a constructor that takes 6-bit input
// clamps anything above 63, which turns an 8-bit color into a washed out
// approximation of itself rather than into an error. This is the conversion to
// put at that boundary, so a color named on the 8-bit scale can be handed to a
// 6-bit one and be seen to have been converted
//
RETRO_Palette RETRO_To6bitColor(RETRO_Palette color)
{
	RETRO_Palette palette;
	palette.r = color.r >> 2;
	palette.g = color.g >> 2;
	palette.b = color.b >> 2;
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

//
// Plot and read a pixel, unclipped. A caller that can leave the screen has to
// say so itself. Building with -DRETRO_DEBUG_BOUNDS aborts at the offending
// call instead; off by default, so the release path keeps no test.
//
#ifdef RETRO_DEBUG_BOUNDS
#include <assert.h>
#define RETRO_ASSERT_PIXEL(x, y) assert((x) >= 0 && (x) < RETRO_WIDTH && (y) >= 0 && (y) < RETRO_HEIGHT)
#else
#define RETRO_ASSERT_PIXEL(x, y) ((void)0)
#endif

void RETRO_PutPixel(int x, int y, unsigned char color)
{
	RETRO_ASSERT_PIXEL(x, y);
	RETRO.framebuffer[RETRO.yoffset[y] + x] = color;
}

unsigned char RETRO_GetPixel(int x, int y)
{
	RETRO_ASSERT_PIXEL(x, y);
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
	return id >= 0 && id < RETRO_MAX_IMAGES && RETRO.image[id] ? RETRO.image[id]->data : NULL;
}

RETRO_Palette *RETRO_ImagePalette(int id = 0)
{
	return id >= 0 && id < RETRO_MAX_IMAGES && RETRO.image[id] ? RETRO.image[id]->palette : NULL;
}

RETRO_Image *RETRO_AllocateImage(void)
{
	int id = 0;
	while (id < RETRO_MAX_IMAGES && RETRO.image[id]) {
		id++;
	}
	if (id == RETRO_MAX_IMAGES) {
		RETRO_RageQuit("Too many images to fit the image list\n");
	}

	RETRO_Image *image = (RETRO_Image *)malloc(sizeof(RETRO_Image));
	if (image == NULL) {
		RETRO_RageQuit("Cannot allocate image memory\n");
	}

	RETRO.image[id] = image;
	RETRO.images++;

	return image;
}

void RETRO_FreeImage(int id = 0)
{
	if (id >= 0 && id < RETRO_MAX_IMAGES && RETRO.image[id]) {
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

bool RETRO_KeyState(SDL_Scancode key)
{
	return RETRO.keystate[key];
}

bool RETRO_KeyPressed(SDL_Scancode key)
{
	if (key > 255) return false;
	// Latched on SDL_EVENT_KEY_DOWN so a down+up that lands in one poll still counts.
	if (RETRO.keylatched[key]) {
		RETRO.keylatched[key] = false;
		return true;
	}
	return false;
}

void RETRO_Quit(void)
{
	RETRO.quit = true;
}

bool RETRO_QuitRequested(void)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			RETRO.quit = true;
		} else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
			if (event.key.scancode < 256) {
				RETRO.keylatched[event.key.scancode] = true;
			}
		}
	}
	RETRO.keystate = SDL_GetKeyboardState(NULL);
	if (RETRO.quit) {
		return true;
	} else if (RETRO.keystate && RETRO.keystate[SDL_SCANCODE_ESCAPE]) {
		return true;
	} else if (RETRO.keystate && RETRO.keystate[SDL_SCANCODE_Q]) {
		return true;
	}
	return false;
}

// *******************************************************************
// Private functions
// *******************************************************************

//
// Advance the simulation by as many whole fixed steps as the elapsed time allows
//
//   acc' = min(acc + dt, STEP * MAX_STEPS)
//   while acc >= STEP:  DEMO_FixedUpdate(STEP);  acc -= STEP
//
// The leftover stays in acc, so the simulation keeps the wall clock without
// drifting. The cap stops a stall from demanding an unbounded burst of
// catch-up. Past it the simulation runs slow instead.
//
void RETRO_AdvanceSimulation(double deltatime)
{
	RETRO.accumulator = MIN(RETRO.accumulator + deltatime, RETRO_SIMULATION_STEP * RETRO_MAX_SIMULATION_STEPS);

	while (RETRO.accumulator >= RETRO_SIMULATION_STEP) {
		RETRO.accumulator -= RETRO_SIMULATION_STEP;

		DEMO_FixedUpdate(RETRO_SIMULATION_STEP);
	}
}

void RETRO_Mainloop(void)
{
	while (!RETRO_QuitRequested()) {
		double deltatime = RETRO_DeltaTime();

		// Check events
		if (RETRO.keystate[SDL_SCANCODE_SPACE]) {
			continue;
		}

		// Advance the displayed clock. Leads the render, so the first frame is
		// handed its own deltatime rather than zero.
		RETRO.time += deltatime;

		// Time the whole frame, for the FPS cap below
		unsigned long int start = SDL_GetTicks();

		// Advance simulation. Both clocks are stepped after the pause check, so
		// time spent paused is discarded rather than caught up on.
		if (DEMO_FixedUpdate) RETRO_AdvanceSimulation(deltatime);

		// Render scene
		if (DEMO_Render) {
			RETRO_Clear();
			DEMO_Render(RETRO.time, deltatime);
			RETRO_Flip();
		} else if (DEMO_Render2) {
			DEMO_Render2(RETRO.time, deltatime);
		} else {
			// A demo that only runs DEMO_FixedUpdate has drawn straight into the framebuffer, which is
			// left standing between frames, so all that remains is to show it
			RETRO_Flip();
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
