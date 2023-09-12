//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETRO_H_
#define _RETRO_H_

#include <SDL2/SDL.h>
#include <getopt.h> // getopt_long
#include <libgen.h> // basename
#include <limits.h> // INT_MIN
#include <math.h> // cos, sin, pow
#include <stdio.h> // FILE
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

#define RETRO_WIDTH 320
#define RETRO_HEIGHT 240
#define RETRO_COLORS 256

#define RETRO_MAX_IMAGES 10

#define RAD2DEG (M_PI / 180)
#define RANDOM(n) (((float)rand() / (float)RAND_MAX) * (n))
#define CLAMP(n, l, h) ((n) < (l) ? (l) : ((n) > ((h) - 1) ? ((h) - 1) : (int)(n)))
#define CLAMP64(n) ((n) < 0 ? 0 : ((n) > 63 ? 63 : (int)(n)))
#define CLAMP128(n) ((n) < 0 ? 0 : ((n) > 127 ? 127 : (int)(n)))
#define CLAMP256(n) ((n) < 0 ? 0 : ((n) > 255 ? 255 : (int)(n)))
#define CLAMPWIDTH(n) ((n) < 0 ? 0 : ((n) > RETRO_WIDTH - 1 ? RETRO_WIDTH - 1 : (int)(n)))
#define CLAMPHEIGHT(n) ((n) < 0 ? 0 : ((n) > RETRO_HEIGHT - 1 ? RETRO_HEIGHT - 1 : (int)(n)))
#define WRAP(n, h) ((int)(n) % (h))
#define WRAP128(n) ((int)(n) & 127)
#define WRAP256(n) ((int)(n) & 255)
#define WRAPWIDTH(n) ((int)(n) % RETRO_WIDTH)
#define WRAPHEIGHT(n) ((int)(n) % RETRO_HEIGHT)
#define SWAP(x, y) do { typeof(x) _SWAP = x; x = y; y = _SWAP; } while (0)

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
	bool vsync;
	bool linear;
	bool showcursor;
	bool showfps;
	int fpscap;
	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	SDL_Texture *renderbuffer = NULL;
	unsigned char *framebuffer = NULL;
	unsigned int palette[RETRO_COLORS];
	RETRO_Image *image[RETRO_MAX_IMAGES];
	int images = 0;
	const unsigned char *keystate;
	bool keydown[256];
	int yoffset[RETRO_HEIGHT];
} RETRO = { .mode = RETRO_MODE_FULLSCREEN, .vsync = true, .showfps = true };

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

void RETRO_SetColor(int color, unsigned char r, unsigned char g, unsigned char b)
{
	RETRO.palette[color] = (r << 16) | (g << 8) | (b);
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
		unsigned char r = (palette[i].r & 63) << 2;
		unsigned char g = (palette[i].g & 63) << 2;
		unsigned char b = (palette[i].b & 63) << 2;
		RETRO_SetColor(i, r, g, b);
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

void RETRO_Clear(unsigned char color = 0)
{
	memset(RETRO.framebuffer, color, RETRO_WIDTH * RETRO_HEIGHT);
}

void RETRO_Blit(unsigned char *src, int size = RETRO_WIDTH * RETRO_HEIGHT, unsigned char *dest = RETRO.framebuffer)
{
	memcpy(dest, src, size);
}

int *RETRO_Yoffset(void)
{
	return RETRO.yoffset;
}

unsigned char *RETRO_ImageData(int id = 0)
{
	return RETRO.image[id] != NULL ? RETRO.image[id]->data : NULL;
}

RETRO_Palette *RETRO_ImagePalette(int id = 0)
{
	return RETRO.image[id] != NULL ? RETRO.image[id]->palette : NULL;
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

RETRO_Image *RETRO_LoadImage(const char *filename)
{
	RETRO_Image *image = RETRO_AllocateImage();

	// Open file
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	// Read header
	unsigned char header[128];
	fread(header, 128, 1, fp);
	if (header[0] != 10) {
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
	int index = 0;
	while (index < image->width * image->height) {
		unsigned char data = getc(fp);
		if (data < 192) {
			image->data[index++] = data;
		} else {
			unsigned char num = data - 192;
			data = getc(fp);
			while (num-- > 0) {
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

	return image;
}

void RETRO_Flip(void)
{
	// Copy framebuffer
	unsigned int *pixels, pitch;
	SDL_LockTexture(RETRO.renderbuffer, NULL, (void **)&pixels, (int *)&pitch);
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		pixels[i] = RETRO.palette[RETRO.framebuffer[i]];
	}
	SDL_UnlockTexture(RETRO.renderbuffer);

	SDL_RenderClear(RETRO.renderer);
	SDL_RenderCopy(RETRO.renderer, RETRO.renderbuffer, NULL, NULL);
	SDL_RenderPresent(RETRO.renderer);
}

void RETRO_Initialize(void)
{
	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		RETRO_RageQuit("SDL_Init failed: %s\n", SDL_GetError());
	}

	// Get current display mode
	SDL_DisplayMode dm;
	if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
		RETRO_RageQuit("SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
	}

	// Set size of window
	if (RETRO.mode == RETRO_MODE_WINDOW) {
		dm.w = RETRO_WIDTH;
		dm.h = RETRO_HEIGHT;
	}

	// Create window title
	char title[128];
	snprintf(title, 128, "RETRO - %s", RETRO.basename);

	// Create window
	RETRO.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, dm.w, dm.h, 0);
	if (RETRO.window == NULL) {
		RETRO_RageQuit("SDL_CreateWindow failed: %s\n", SDL_GetError());
	}

	// Create renderer
	unsigned int flags = SDL_RENDERER_ACCELERATED;
	if (RETRO.vsync) {
		flags |= SDL_RENDERER_PRESENTVSYNC;
	}
	if (RETRO.linear) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	}
	RETRO.renderer = SDL_CreateRenderer(RETRO.window, -1, flags);
	SDL_RenderSetLogicalSize(RETRO.renderer, RETRO_WIDTH, RETRO_HEIGHT);

	// Set fullscreen
	if (RETRO.mode == RETRO_MODE_FULLSCREEN) {
		SDL_SetWindowFullscreen(RETRO.window, SDL_WINDOW_FULLSCREEN);
	}

	// Create render buffer
	RETRO.renderbuffer = SDL_CreateTexture(RETRO.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RETRO_WIDTH, RETRO_HEIGHT);

	// Create framebuffer
	RETRO.framebuffer = (unsigned char *)malloc(RETRO_WIDTH * RETRO_HEIGHT);
	if (RETRO.framebuffer == NULL) {
		RETRO_RageQuit("Cannot allocate framebuffer memory\n");
	}
	memset(RETRO.framebuffer, 0, RETRO_WIDTH * RETRO_HEIGHT);

	// Cursor
	SDL_ShowCursor(RETRO.showcursor);

	// Build Y offset table
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		RETRO.yoffset[y] = y * RETRO_WIDTH;
	}

	if (RETRO_Initialize_3D != NULL) RETRO_Initialize_3D();
}

void RETRO_Deinitialize(void)
{
	if (RETRO_Deinitialize_3D != NULL) RETRO_Deinitialize_3D();

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
	SDL_RenderSetVSync(RETRO.renderer, state);
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

bool RETRO_QuitRequested(void)
{
	SDL_PumpEvents();
	RETRO.keystate = SDL_GetKeyboardState(NULL);
	if (SDL_QuitRequested()) {
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

		// Render scene
		unsigned long int start = SDL_GetTicks64();
		if (DEMO_Render != NULL) {
			RETRO_Clear();
			DEMO_Render(deltatime);
			RETRO_Flip();
		} else if (DEMO_Render2 != NULL) {
			DEMO_Render2(deltatime);
		}
		unsigned long int stop = SDL_GetTicks64();

		// Limit FPS
		if (RETRO.fpscap && ((stop - start) < 1000UL / RETRO.fpscap)) {
			SDL_Delay((1000 / RETRO.fpscap) - (stop - start));
		}

		// Show FPS once a second
		if (RETRO.showfps) {
			static unsigned long int fpsticks = SDL_GetTicks64();
			static int fpscount = 0;
			if (fpsticks < SDL_GetTicks64() - 1000UL) {
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
