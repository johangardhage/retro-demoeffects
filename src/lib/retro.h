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

// *******************************************************************
// Public dynamic functions
// *******************************************************************

void __attribute__((weak)) DEMO_Startup();
void __attribute__((weak)) DEMO_Initialize();
void __attribute__((weak)) DEMO_Deinitialize();
void __attribute__((weak)) DEMO_Render(double deltatime);
void __attribute__((weak)) DEMO_Render2(double deltatime);

// *******************************************************************
// Private dynamic functions
// *******************************************************************

void __attribute__((weak)) RETRO_Deinitialize_3D();

// *******************************************************************
// Public variables
// *******************************************************************

#define RETRO_WIDTH 320
#define RETRO_HEIGHT 240
#define RETRO_COLORS 256

#define RETRO_MAX_TEXTURES 5

#define RAD2DEG (M_PI / 180)
#define RANDOM(n) (((float)rand() / (float)RAND_MAX) * (n))
#define CLAMP(n, l, h) ((n) < (l) ? (l) : ((n) > ((h) - 1) ? ((h) - 1) : (n)))
#define CLAMP128(n) ((n) < 0 ? 0 : ((n) > 127 ? 127 : (n)))
#define CLAMP256(n) ((n) < 0 ? 0 : ((n) > 255 ? 255 : (n)))
#define CLAMPWIDTH(n) ((n) < 0 ? 0 : ((n) > RETRO_WIDTH - 1 ? RETRO_WIDTH - 1 : (n)))
#define CLAMPHEIGHT(n) ((n) < 0 ? 0 : ((n) > RETRO_HEIGHT - 1 ? RETRO_HEIGHT - 1 : (n)))
#define WRAP128(n) ((int)(n) & 127)
#define WRAP256(n) ((int)(n) & 255)
#define WRAPWIDTH(n) ((int)(n) % RETRO_WIDTH)
#define WRAPHEIGHT(n) ((int)(n) % RETRO_HEIGHT)
#define SWAP(x, y) do { typeof(x) _SWAP = x; x = y; y = _SWAP; } while (0)

struct Palette {
	unsigned char r, g, b;
};

struct Texture {
	unsigned char header[128];
	Palette palette[256];
	unsigned char *image;
	int width;
	int height;
	~Texture() {
		if (image != NULL) {
			free(image);
		}
	}
};

struct Point2Df {
	float x, y;
};

struct Point2D {
	int x, y;
};

// *******************************************************************
// Private variables
// *******************************************************************

struct RETRO_Lib {
	enum { FULLSCREEN, WINDOW, FULLWINDOW } mode;
	char *basename;
	bool vsync;
	bool linear;
	bool showcursor;
	bool showfps;
	int fpscap;
	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	SDL_Surface *surface = NULL;
	SDL_Texture *surfacetexture = NULL;
	unsigned char *surfacebuffer = NULL;
	int yoffsettable[RETRO_HEIGHT];
	SDL_Color colorpalette[RETRO_COLORS];
	Uint32 colortable[RETRO_COLORS];
	Texture *texture[RETRO_MAX_TEXTURES];
	int textures = 0;
};

RETRO_Lib RETRO_lib = { .mode = RETRO_Lib::FULLSCREEN, .vsync = true, .showfps = true };

// *******************************************************************
// Public methods
// *******************************************************************

void RETRO_RageQuit(const char *message, const char *error)
{
	printf(message, error);
	exit(-1);
}

Texture *RETRO_AllocateTexture(void)
{
	int id = RETRO_lib.textures++;
	RETRO_lib.texture[id] = new Texture();
	return RETRO_lib.texture[id];
}

Texture *RETRO_GetTexture(int id = 0)
{
	return RETRO_lib.texture[id];
}

unsigned char *RETRO_GetTextureImage(int id = 0)
{
	return RETRO_lib.texture[id] != NULL ? RETRO_lib.texture[id]->image : NULL;
}

Palette *RETRO_GetTexturePalette(int id = 0)
{
	return RETRO_lib.texture[id] != NULL ? RETRO_lib.texture[id]->palette : NULL;
}

unsigned char *RETRO_GetSurfaceBuffer(void)
{
	return RETRO_lib.surfacebuffer;
}

int *RETRO_GetYOffsetTable(void)
{
	return RETRO_lib.yoffsettable;
}

void RETRO_PutPixel(int x, int y, unsigned char color)
{
	RETRO_lib.surfacebuffer[RETRO_lib.yoffsettable[y] + x] = color;
}

unsigned char RETRO_GetPixel(int x, int y)
{
	return (RETRO_lib.surfacebuffer[RETRO_lib.yoffsettable[y] + x]);
}

void RETRO_Clear(unsigned char color = 0)
{
	memset(RETRO_lib.surfacebuffer, color, RETRO_WIDTH * RETRO_HEIGHT);
}

void RETRO_Blit(unsigned char *src, int size)
{
	memcpy(RETRO_lib.surfacebuffer, src, size);
}

void RETRO_Copy(unsigned char *src, unsigned char *dst, int size)
{
	memcpy(dst, src, size);
}

void RETRO_SetColor(unsigned char color, unsigned char r, unsigned char g, unsigned char b)
{
	RETRO_lib.colorpalette[color].r = r;
	RETRO_lib.colorpalette[color].g = g;
	RETRO_lib.colorpalette[color].b = b;
	RETRO_lib.colortable[color] = SDL_MapRGB(RETRO_lib.surface->format, r, g, b);
}

SDL_Color RETRO_GetColor(unsigned char color)
{
	return RETRO_lib.colorpalette[color];
}

void RETRO_SetPalette(void)
{
	SDL_SetPaletteColors(RETRO_lib.surface->format->palette, RETRO_lib.colorpalette, 0, RETRO_COLORS);
}

void RETRO_SetPalette(Palette *palette)
{
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, palette->r, palette->g, palette->b);
		palette++;
	}
	RETRO_SetPalette();
}

void RETRO_SetPaletteFromTexture(int id = 0)
{
	RETRO_SetPalette(RETRO_GetTexturePalette(id));
}

void RETRO_SetBlackPalette(void)
{
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, 0, 0, 0);
	}
	RETRO_SetPalette();
}

bool RETRO_FadeIn(int steps, int step, Palette *palette)
{
	if (step >= steps) return true;

	for (int i = 0; i < RETRO_COLORS; i++) {
		unsigned char r = (float)palette[i].r / steps * step;
		unsigned char g = (float)palette[i].g / steps * step;
		unsigned char b = (float)palette[i].b / steps * step;
		RETRO_SetColor(i, r, g, b);
	}
	RETRO_SetPalette();

	return false;
}

bool RETRO_FadeOut(int steps, int step, Palette *palette)
{
	if (step >= steps) return true;

	for (int i = 0; i < RETRO_COLORS; i++) {
		unsigned char r = (float)palette[i].r / steps * (steps - step);
		unsigned char g = (float)palette[i].g / steps * (steps - step);
		unsigned char b = (float)palette[i].b / steps * (steps - step);
		RETRO_SetColor(i, r, g, b);
	}
	RETRO_SetPalette();

	return false;
}

Texture *RETRO_LoadTexture(const char *filename)
{
	Texture *texture = RETRO_AllocateTexture();

	// Open file
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	// Read header
	fread(texture->header, 128, 1, fp);
	if (texture->header[0] != 10) {
		RETRO_RageQuit("Cannot read file: %s\n", filename);
	}

	// From header data, build some image info
	int xmin = (texture->header[4] + (texture->header[5] << 8));
	int ymin = (texture->header[6] + (texture->header[7] << 8));
	int xmax = (texture->header[8] + (texture->header[9] << 8));
	int ymax = (texture->header[10] + (texture->header[11] << 8));

	// Calculate the size of image
	texture->width = xmax - xmin + 1;
	texture->height = ymax - ymin + 1;

	// Reserve memory
	texture->image = (unsigned char *)malloc(texture->width * texture->height);

	// Unpack image
	int index = 0;
	while (index < texture->width * texture->height) {
		unsigned char data = getc(fp);
		if (data < 192) {
			texture->image[index++] = data;
		} else {
			unsigned char num = data - 192;
			data = getc(fp);
			while (num-- > 0) {
				texture->image[index++] = data;
			}
		}
	}

	// Read palette from end of file
	fseek(fp, -768, SEEK_END);
	for (int i = 0; i < 256; i++) {
		texture->palette[i].r = fgetc(fp);
		texture->palette[i].g = fgetc(fp);
		texture->palette[i].b = fgetc(fp);
	}

	// Close file
	fclose(fp);

	return texture;
}

void RETRO_Flip()
{
	void *pixels;
	int pitch;

	// Copy surface buffer to surface
	SDL_LockTexture(RETRO_lib.surfacetexture, NULL, &pixels, &pitch);
	Uint32 *pixels_ptr = (Uint32 *)pixels;
	for (int i = 0; i < RETRO_HEIGHT * RETRO_WIDTH; i++) {
		pixels_ptr[i] = RETRO_lib.colortable[RETRO_lib.surfacebuffer[i]];
	}
	SDL_UnlockTexture(RETRO_lib.surfacetexture);

	SDL_RenderClear(RETRO_lib.renderer);
	SDL_RenderCopy(RETRO_lib.renderer, RETRO_lib.surfacetexture, NULL, NULL);
	SDL_RenderPresent(RETRO_lib.renderer);
}

void RETRO_Initialize()
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
	if (RETRO_lib.mode == RETRO_Lib::WINDOW) {
		dm.w = RETRO_WIDTH;
		dm.h = RETRO_HEIGHT;
	}

	// Create window title
	char title[256];
	snprintf(title, 256, "RETRO - %s", RETRO_lib.basename);

	// Create window
	RETRO_lib.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, dm.w, dm.h, 0);
	if (RETRO_lib.window == NULL) {
		RETRO_RageQuit("SDL_CreateWindow failed: %s\n", SDL_GetError());
	}

	// Cursor
	if (RETRO_lib.showcursor) {
		SDL_ShowCursor(1);
	} else {
		SDL_ShowCursor(0);
	}

	// Create renderer
	Uint32 flags = SDL_RENDERER_ACCELERATED;
	if (RETRO_lib.vsync) {
		flags |= SDL_RENDERER_PRESENTVSYNC;
	}
	if (RETRO_lib.linear) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	}
	RETRO_lib.renderer = SDL_CreateRenderer(RETRO_lib.window, -1, flags);
	SDL_RenderSetLogicalSize(RETRO_lib.renderer, RETRO_WIDTH, RETRO_HEIGHT);

	// Set fullscreen
	if (RETRO_lib.mode == RETRO_Lib::FULLSCREEN) {
		SDL_SetWindowFullscreen(RETRO_lib.window, SDL_WINDOW_FULLSCREEN);
	}

	// Create surface and surface texture
	RETRO_lib.surface = SDL_CreateRGBSurface(0, RETRO_WIDTH, RETRO_HEIGHT, 32, 0, 0, 0, 0);
	RETRO_lib.surfacetexture = SDL_CreateTexture(RETRO_lib.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RETRO_WIDTH, RETRO_HEIGHT);

	// Create surface buffer
	RETRO_lib.surfacebuffer = (unsigned char *)malloc(RETRO_WIDTH * RETRO_HEIGHT);
	memset(RETRO_lib.surfacebuffer, 0, RETRO_WIDTH * RETRO_HEIGHT);

	// Build Y offset table
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		RETRO_lib.yoffsettable[y] = y * RETRO_WIDTH;
	}

	// Initialize demo
	if (DEMO_Initialize != NULL) DEMO_Initialize();
}

void RETRO_Deinitialize()
{
	// Deinitialize demo
	if (DEMO_Deinitialize != NULL) DEMO_Deinitialize();

	// Deinitialize 3D
	if (RETRO_Deinitialize_3D != NULL) RETRO_Deinitialize_3D();

	for (int i = 0; i < RETRO_lib.textures; i++) {
		if (RETRO_lib.texture[i]) delete RETRO_lib.texture[i];
	}
	if (RETRO_lib.surfacebuffer) free(RETRO_lib.surfacebuffer);
	SDL_DestroyTexture(RETRO_lib.surfacetexture);
	SDL_FreeSurface(RETRO_lib.surface);
	SDL_DestroyRenderer(RETRO_lib.renderer);
	SDL_DestroyWindow(RETRO_lib.window);
	SDL_Quit();
}

void RETRO_Mainloop()
{
	// Initialize delta time
	Uint32 now_counter = SDL_GetPerformanceCounter();
	Uint32 last_counter = 0;

	char done = 0;
	while (!done) {
		// Update delta time
		last_counter = now_counter;
		now_counter = SDL_GetPerformanceCounter();
		double delta_time = (double)(now_counter - last_counter) / SDL_GetPerformanceFrequency();

		// Update keys
		SDL_PumpEvents();
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		if (SDL_QuitRequested()) {
			done = 1;
			break;
		}
		if (keys[SDL_GetScancodeFromKey(SDLK_ESCAPE)]) {
			done = 1;
			break;
		}
		if (keys[SDL_GetScancodeFromKey(SDLK_q)]) {
			done = 1;
			break;
		}

		if (keys[SDL_GetScancodeFromKey(SDLK_SPACE)]) {
			continue;
		}

		// Update render buffer
		Uint32 render_begin = SDL_GetTicks();
		if (DEMO_Render != NULL) {
			RETRO_Clear();
			DEMO_Render(delta_time);
			RETRO_Flip();
		} else if (DEMO_Render2 != NULL) {
			DEMO_Render2(delta_time);
		}
		Uint32 render_end = SDL_GetTicks();

		// Limit FPS
		if (RETRO_lib.fpscap && ((render_end - render_begin) < (Uint32)1000 / RETRO_lib.fpscap)) {
			SDL_Delay((1000 / RETRO_lib.fpscap) - (render_end - render_begin));
		}

		// Print FPS
		static Uint32 fpslasttime = SDL_GetTicks();
		static Uint32 fpscounter = 0;
		if (fpslasttime < SDL_GetTicks() - 1 * 1000) {
			if (RETRO_lib.showfps) {
				char title[256];
				snprintf(title, 256, "RETRO - %s - FPS: %d", RETRO_lib.basename, fpscounter);
				SDL_SetWindowTitle(RETRO_lib.window, title);
			}
			fpslasttime = SDL_GetTicks();
			fpscounter = 0;
		}
		fpscounter++;
	}
}

#endif
