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

void __attribute__((weak)) DEMO_Startup(void);
void __attribute__((weak)) DEMO_Initialize(void);
void __attribute__((weak)) DEMO_Deinitialize(void);
void __attribute__((weak)) DEMO_Render(double deltatime);
void __attribute__((weak)) DEMO_Render2(double deltatime);

// *******************************************************************
// Private dynamic functions
// *******************************************************************

void __attribute__((weak)) RETRO_Deinitialize_3D(void);

// *******************************************************************
// Public variables
// *******************************************************************

#define RETRO_WIDTH 320
#define RETRO_HEIGHT 240
#define RETRO_COLORS 256

#define RETRO_MAX_TEXTURES 5

#define RAD2DEG (M_PI / 180)
#define RANDOM(n) (((float)rand() / (float)RAND_MAX) * (n))
#define CLAMP(n, l, h) ((n) < (l) ? (l) : ((n) > ((h) - 1) ? ((h) - 1) : (int)(n)))
#define CLAMP128(n) ((n) < 0 ? 0 : ((n) > 127 ? 127 : (int)(n)))
#define CLAMP256(n) ((n) < 0 ? 0 : ((n) > 255 ? 255 : (int)(n)))
#define CLAMPWIDTH(n) ((n) < 0 ? 0 : ((n) > RETRO_WIDTH - 1 ? RETRO_WIDTH - 1 : (int)(n)))
#define CLAMPHEIGHT(n) ((n) < 0 ? 0 : ((n) > RETRO_HEIGHT - 1 ? RETRO_HEIGHT - 1 : (int)(n)))
#define WRAP128(n) ((int)(n) & 127)
#define WRAP256(n) ((int)(n) & 255)
#define WRAPWIDTH(n) ((int)(n) % RETRO_WIDTH)
#define WRAPHEIGHT(n) ((int)(n) % RETRO_HEIGHT)
#define SWAP(x, y) do { typeof(x) _SWAP = x; x = y; y = _SWAP; } while (0)

struct Palette {
	unsigned char r, g, b;
};

struct Texture {
	Palette palette[256];
	unsigned char *image;
	int width;
	int height;
};

// *******************************************************************
// Private variables
// *******************************************************************

enum {
	RETRO_MODE_FULLSCREEN,
	RETRO_MODE_WINDOW,
	RETRO_MODE_FULLWINDOW
};

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
	SDL_Texture *surfacebuffer = NULL;
	unsigned char *framebuffer = NULL;
	unsigned int palette[RETRO_COLORS];
	Texture *texture[RETRO_MAX_TEXTURES];
	int textures = 0;
	const unsigned char *keystate;
	int yoffset[RETRO_HEIGHT];
} RETRO = { .mode = RETRO_MODE_FULLSCREEN, .vsync = true, .showfps = true };

// *******************************************************************
// Public methods
// *******************************************************************

void RETRO_RageQuit(const char *message, const char *error)
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
	RETRO.palette[color] = (b << 16) | (g << 8) | (r);
}

void RETRO_SetPalette(Palette *palette)
{
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, palette[i].r, palette[i].g, palette[i].b);
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

void RETRO_Blit(unsigned char *src, unsigned char *dest = RETRO.framebuffer, int size = RETRO_WIDTH * RETRO_HEIGHT)
{
	memcpy(dest, src, size);
}

int *RETRO_Yoffset(void)
{
	return RETRO.yoffset;
}

Texture *RETRO_Texture(int id = 0)
{
	return RETRO.texture[id];
}

unsigned char *RETRO_TextureImage(int id = 0)
{
	return RETRO.texture[id] != NULL ? RETRO.texture[id]->image : NULL;
}

Palette *RETRO_TexturePalette(int id = 0)
{
	return RETRO.texture[id] != NULL ? RETRO.texture[id]->palette : NULL;
}

Texture *RETRO_AllocateTexture(void)
{
	RETRO.texture[RETRO.textures] = (Texture *)malloc(sizeof(Texture));
	if (RETRO.texture[RETRO.textures] == NULL) {
		RETRO_RageQuit("Cannot allocate texture memory\n", "");
	}
	return RETRO.texture[RETRO.textures++];
}

void RETRO_FreeTexture(int id = 0)
{
	if (RETRO.texture[id]) {
		if (RETRO.texture[id]->image) {
			free(RETRO.texture[id]->image);
			RETRO.texture[id]->image = NULL;
		}
		free(RETRO.texture[id]);
		RETRO.texture[id] = NULL;
		RETRO.textures--;
	}
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
	texture->width = xmax - xmin + 1;
	texture->height = ymax - ymin + 1;

	// Reserve memory
	texture->image = (unsigned char *)malloc(texture->width * texture->height);
	if (texture->image == NULL) {
		RETRO_RageQuit("Cannot allocate texture image memory\n", "");
	}

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

void RETRO_Flip(void)
{
	unsigned int *pixels;
	int pitch;

	// Copy framebuffer
	SDL_LockTexture(RETRO.surfacebuffer, NULL, (void **)&pixels, &pitch);
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		pixels[i] = RETRO.palette[RETRO.framebuffer[i]];
	}
	SDL_UnlockTexture(RETRO.surfacebuffer);

	SDL_RenderClear(RETRO.renderer);
	SDL_RenderCopy(RETRO.renderer, RETRO.surfacebuffer, NULL, NULL);
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
	char title[256];
	snprintf(title, 256, "RETRO - %s", RETRO.basename);

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

	// Create surface buffer
	RETRO.surfacebuffer = SDL_CreateTexture(RETRO.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, RETRO_WIDTH, RETRO_HEIGHT);

	// Create framebuffer
	RETRO.framebuffer = (unsigned char *)malloc(RETRO_WIDTH * RETRO_HEIGHT);
	if (RETRO.framebuffer == NULL) {
		RETRO_RageQuit("Cannot allocate framebuffer memory\n", "");
	}
	memset(RETRO.framebuffer, 0, RETRO_WIDTH * RETRO_HEIGHT);

	// Cursor
	SDL_ShowCursor(RETRO.showcursor);

	// Build Y offset table
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		RETRO.yoffset[y] = y * RETRO_WIDTH;
	}
}

void RETRO_Deinitialize(void)
{
	// Deinitialize 3D
	if (RETRO_Deinitialize_3D != NULL) RETRO_Deinitialize_3D();

	// Free textures
	for (int i = 0; i < RETRO_MAX_TEXTURES; i++) {
		RETRO_FreeTexture(i);
	}

	if (RETRO.framebuffer) {
		free(RETRO.framebuffer);
	}

	SDL_DestroyTexture(RETRO.surfacebuffer);
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
	// Initialize delta time
	static unsigned int current = SDL_GetPerformanceCounter();
	static unsigned int previous = 0;

	// Update delta time
	previous = current;
	current = SDL_GetPerformanceCounter();

	return (double)(current - previous) / SDL_GetPerformanceFrequency();
}

bool RETRO_KeyState(SDL_KeyCode keycode)
{
	return RETRO.keystate[SDL_GetScancodeFromKey(keycode)];
}

bool RETRO_QuitRequested(void)
{
	SDL_PumpEvents();
	RETRO.keystate = SDL_GetKeyboardState(NULL);
	if (SDL_QuitRequested()) {
		return true;
	} else if (RETRO.keystate[SDL_GetScancodeFromKey(SDLK_ESCAPE)]) {
		return true;
	} else if (RETRO.keystate[SDL_GetScancodeFromKey(SDLK_q)]) {
		return true;
	}
	return false;
}

// *******************************************************************
// Private methods
// *******************************************************************

void RETRO_Mainloop(void)
{
	while (!RETRO_QuitRequested()) {
		// Calculate delta time
		double deltatime = RETRO_DeltaTime();

		// Check events
		if (RETRO.keystate[SDL_GetScancodeFromKey(SDLK_SPACE)]) {
			continue;
		}

		// Render scene
		unsigned int start = SDL_GetTicks();
		if (DEMO_Render != NULL) {
			RETRO_Clear();
			DEMO_Render(deltatime);
			RETRO_Flip();
		} else if (DEMO_Render2 != NULL) {
			DEMO_Render2(deltatime);
		}
		unsigned int stop = SDL_GetTicks();

		// Limit FPS
		if (RETRO.fpscap && ((stop - start) < (unsigned int)1000 / RETRO.fpscap)) {
			SDL_Delay((1000 / RETRO.fpscap) - (stop - start));
		}

		// Print FPS
		static unsigned int fpslasttime = SDL_GetTicks();
		static unsigned int fpscounter = 0;
		if (fpslasttime < SDL_GetTicks() - 1 * 1000) {
			if (RETRO.showfps) {
				char title[256];
				snprintf(title, 256, "RETRO - %s - FPS: %d", RETRO.basename, fpscounter);
				SDL_SetWindowTitle(RETRO.window, title);
			}
			fpslasttime = SDL_GetTicks();
			fpscounter = 0;
		}
		fpscounter++;
	}
}

#endif
