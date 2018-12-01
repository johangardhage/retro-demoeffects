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

enum {
	RETRO_WINDOW = 1,
	RETRO_FULLWINDOW = 2,
	RETRO_FULLSCREEN = 4,
	RETRO_VSYNC = 8,
	RETRO_LINEAR = 16,
	RETRO_SHOWCURSOR = 32,
	RETRO_SHOWFPS = 64
};

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

char *RETRO_basename;
int RETRO_parameters;
int RETRO_fpscap = 0;

SDL_Window *RETRO_window = NULL;
SDL_Renderer *RETRO_renderer = NULL;
SDL_Surface *RETRO_surface = NULL;
SDL_Texture *RETRO_surfacetexture = NULL;

unsigned char *RETRO_surfacebuffer = NULL;
int RETRO_yoffsettable[RETRO_HEIGHT];
SDL_Color RETRO_colorpalette[RETRO_COLORS];
Uint32 RETRO_colortable[RETRO_COLORS];
Texture *RETRO_texture[RETRO_MAX_TEXTURES];
int RETRO_textures = 0;

// *******************************************************************
// Public methods
// *******************************************************************

void RETRO_RageQuit(const char *message, const char *error)
{
	printf(message, error);
	exit(-1);
}

void RETRO_PrintFPS(void)
{
	static Uint32 fpslasttime = SDL_GetTicks();
	static Uint32 fpscounter = 0;

	fpscounter++;
	if (fpslasttime < SDL_GetTicks() - 1 * 1000) {
		if (RETRO_parameters & RETRO_SHOWFPS) {
			char title[256];
			snprintf(title, 256, "RETRO - %s - FPS: %d", RETRO_basename, fpscounter);
			SDL_SetWindowTitle(RETRO_window, title);
		}
		fpslasttime = SDL_GetTicks();
		fpscounter = 0;
	}
}

Texture *RETRO_AllocateTexture(void)
{
	int id = RETRO_textures++;
	RETRO_texture[id] = new Texture();
	return RETRO_texture[id];
}

Texture *RETRO_GetTexture(int id = 0)
{
	return RETRO_texture[id];
}

unsigned char *RETRO_GetTextureImage(int id = 0)
{
	return RETRO_texture[id] != NULL ? RETRO_texture[id]->image : NULL;
}

Palette *RETRO_GetTexturePalette(int id = 0)
{
	return RETRO_texture[id] != NULL ? RETRO_texture[id]->palette : NULL;
}

unsigned char *RETRO_GetSurfaceBuffer(void)
{
	return RETRO_surfacebuffer;
}

int *RETRO_GetYOffsetTable(void)
{
	return RETRO_yoffsettable;
}

void RETRO_PutPixel(int x, int y, unsigned char color)
{
	RETRO_surfacebuffer[RETRO_yoffsettable[y] + x] = color;
}

unsigned char RETRO_GetPixel(int x, int y)
{
	return (RETRO_surfacebuffer[RETRO_yoffsettable[y] + x]);
}

void RETRO_Clear(unsigned char color = 0)
{
	memset(RETRO_surfacebuffer, color, RETRO_WIDTH * RETRO_HEIGHT);
}

void RETRO_Blit(unsigned char *src, int size)
{
	memcpy(RETRO_surfacebuffer, src, size);
}

void RETRO_Copy(unsigned char *src, unsigned char *dst, int size)
{
	memcpy(dst, src, size);
}

void RETRO_SetColor(unsigned char color, unsigned char r, unsigned char g, unsigned char b)
{
	RETRO_colorpalette[color].r = r;
	RETRO_colorpalette[color].g = g;
	RETRO_colorpalette[color].b = b;
	RETRO_colortable[color] = SDL_MapRGB(RETRO_surface->format, r, g, b);
}

SDL_Color RETRO_GetColor(unsigned char color)
{
	return RETRO_colorpalette[color];
}

void RETRO_SetPalette(void)
{
	SDL_SetPaletteColors(RETRO_surface->format->palette, RETRO_colorpalette, 0, RETRO_COLORS);
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
	SDL_LockTexture(RETRO_surfacetexture, NULL, &pixels, &pitch);
	Uint32 *pixels_ptr = (Uint32 *)pixels;
	for (int i = 0; i < RETRO_HEIGHT * RETRO_WIDTH; i++) {
		pixels_ptr[i] = RETRO_colortable[RETRO_surfacebuffer[i]];
	}
	SDL_UnlockTexture(RETRO_surfacetexture);

	SDL_RenderClear(RETRO_renderer);
	SDL_RenderCopy(RETRO_renderer, RETRO_surfacetexture, NULL, NULL);
	SDL_RenderPresent(RETRO_renderer);
}

int RETRO_ParseArguments(int argc, char *argv[], int param)
{
	int parameters = param;
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"window", no_argument, 0, 'w'},
		{"fullwindow", no_argument, 0, 0},
		{"fullscreen", no_argument, 0, 'f'},
		{"vsync", no_argument, 0, 'v'},
		{"novsync", no_argument, 0, 0},
		{"linear", no_argument, 0, 'l'},
		{"nolinear", no_argument, 0, 0},
		{"showcursor", no_argument, 0, 'c'},
		{"nocursor", no_argument, 0, 0},
		{"showfps", no_argument, 0, 0},
		{"nofps", no_argument, 0, 0},
		{"capfps", required_argument, 0, 0},
		{0, 0, 0, 0}};
	bool usage = false;
	int c;
	int option_index = 0;
	while ((c = getopt_long(argc, argv, ":hwflcv", long_options, &option_index)) != -1) {
		switch (c) {
		case 0:
			if (strcmp("fullwindow", long_options[option_index].name) == 0) {
				parameters |= RETRO_FULLWINDOW;
				parameters &= ~(RETRO_WINDOW | RETRO_FULLSCREEN);
			} else if (strcmp("novsync", long_options[option_index].name) == 0) {
				parameters &= ~RETRO_VSYNC;
			} else if (strcmp("nolinear", long_options[option_index].name) == 0) {
				parameters &= ~RETRO_LINEAR;
			} else if (strcmp("nocursor", long_options[option_index].name) == 0) {
				parameters &= ~RETRO_SHOWCURSOR;
			} else if (strcmp("showfps", long_options[option_index].name) == 0) {
				parameters |= RETRO_SHOWFPS;
			} else if (strcmp("nofps", long_options[option_index].name) == 0) {
				parameters &= ~RETRO_SHOWFPS;
			} else if (strcmp("capfps", long_options[option_index].name) == 0) {
				RETRO_fpscap = atoi(optarg);
			}
			break;
		case 'h':
			usage = true;
			break;
		case 'w':
			parameters |= RETRO_WINDOW;
			parameters &= ~(RETRO_FULLWINDOW | RETRO_FULLSCREEN);
			break;
		case 'f':
			parameters |= RETRO_FULLSCREEN;
			parameters &= ~(RETRO_WINDOW | RETRO_FULLWINDOW);
			break;
		case 'v':
			parameters |= RETRO_VSYNC;
			break;
		case 'l':
			parameters |= RETRO_LINEAR;
			break;
		case 'c':
			parameters |= RETRO_SHOWCURSOR;
			break;
		case '?':
			usage = true;
			printf("unrecognized option '%s'\n", argv[optind - 1]);
			break;
		default:
			usage = true;
			break;
		}
	}
	if (optind < argc) {
		usage = true;
		printf("non-option ARGV-elements: ");
		while (optind < argc) {
			printf("%s ", argv[optind++]);
		}
		printf("\n");
	}
	if (usage) {
		printf("Usage: %s [OPTION]...\n\n", RETRO_basename);
		printf("Options:\n");
		printf(" -h, --help           Display this text and exit\n");
		printf(" -w, --window         Render in a window\n");
		printf("     --fullwindow     Render in a fullscreen window\n");
		printf(" -f, --fullscreen     Render in fullscreen\n");
		printf(" -v, --vsync          Enable sync to vertical refresh\n");
		printf("     --novsync        Disable sync to vertical refresh\n");
		printf(" -l, --linear         Render using linear filtering\n");
		printf("     --nolinear       Render using nearest pixel sampling\n");
		printf(" -c, --showcursor     Show mouse cursor\n");
		printf("     --nocursor       Hide mouse cursor\n");
		printf("     --showfps        Show frame rate in window title\n");
		printf("     --nofps          Hide frame rate\n");
		printf("     --capfps=VALUE   Limit frame rate to the specified VALUE\n");
		exit(1);
	}
	return parameters;
}

void RETRO_Initialize(int argc, char *argv[], int param)
{
	// Parse arguments
	RETRO_basename = basename(argv[0]);
	RETRO_parameters = RETRO_ParseArguments(argc, argv, param);
	Uint32 flags = 0;

	// Title
	char title[256];
	snprintf(title, 256, "RETRO - %s", RETRO_basename);

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		RETRO_RageQuit("SDL_Init failed: %s\n", SDL_GetError());
	}

	// Get current display mode
	SDL_DisplayMode dm;
	if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
		RETRO_RageQuit("SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
	}
	if (RETRO_parameters & RETRO_WINDOW) {
		dm.w = RETRO_WIDTH;
		dm.h = RETRO_HEIGHT;
	}

	// Cursor
	if (RETRO_parameters & RETRO_SHOWCURSOR) {
		SDL_ShowCursor(1);
	} else {
		SDL_ShowCursor(0);
	}

	// Create window
	flags = 0;
	if (RETRO_parameters & RETRO_FULLSCREEN) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	RETRO_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, dm.w, dm.h, flags);
	if (RETRO_window == NULL) {
		RETRO_RageQuit("SDL_CreateWindow failed: %s\n", SDL_GetError());
	}

	// Create renderer
	flags = SDL_RENDERER_ACCELERATED;
	if (RETRO_parameters & RETRO_VSYNC) {
		flags |= SDL_RENDERER_PRESENTVSYNC;
	}
	if (RETRO_parameters & RETRO_LINEAR) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	}
	RETRO_renderer = SDL_CreateRenderer(RETRO_window, -1, flags);
	SDL_RenderSetLogicalSize(RETRO_renderer, RETRO_WIDTH, RETRO_HEIGHT);

	// Create surface and surface texture
	RETRO_surface = SDL_CreateRGBSurface(0, RETRO_WIDTH, RETRO_HEIGHT, 32, 0, 0, 0, 0);
	RETRO_surfacetexture = SDL_CreateTexture(RETRO_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RETRO_WIDTH, RETRO_HEIGHT);

	// Create surface buffer
	RETRO_surfacebuffer = (unsigned char *)malloc(RETRO_WIDTH * RETRO_HEIGHT);
	memset(RETRO_surfacebuffer, 0, RETRO_WIDTH * RETRO_HEIGHT);

	// Build Y offset table
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		RETRO_yoffsettable[y] = y * RETRO_WIDTH;
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

	for (int i = 0; i < RETRO_textures; i++) {
		if (RETRO_texture[i]) delete RETRO_texture[i];
	}
	if (RETRO_surfacebuffer) free(RETRO_surfacebuffer);
	SDL_DestroyTexture(RETRO_surfacetexture);
	SDL_FreeSurface(RETRO_surface);
	SDL_DestroyRenderer(RETRO_renderer);
	SDL_DestroyWindow(RETRO_window);
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
		if (RETRO_fpscap && ((render_end - render_begin) < (Uint32)1000 / RETRO_fpscap)) {
			SDL_Delay((1000 / RETRO_fpscap) - (render_end - render_begin));
		}

		// Print FPS
		RETRO_PrintFPS();
	}
}

#endif
