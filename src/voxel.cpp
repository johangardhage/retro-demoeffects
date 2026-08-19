//
// Voxel 2
//
// A 256×256 wrapping height map, drawn as a front-to-back fan of depth
// slices. Slice i is a line from heading − FOV to heading + FOV, stepped
// across the screen. Height and colour are sampled with bilinear filtering:
//
//   sample = lerp_v(lerp_u(s00, s10), lerp_u(s01, s11))
//
// A height difference at depth i is a pinhole
//
//   y = H/2 + (h − hy) · scale / (i + 1)
//
// painted down to the highest y already filled, so nearer ground occludes.
// The arrow keys steer and move the camera across the wrapping terrain. By
// default the camera follows the ground; Space toggles a flycam in which R
// and F move vertically. Colour is an 8-bit grey ramp.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define MAP_WIDTH 256
#define MAP_HEIGHT 256
#define VOXEL_FOV ((float)(M_PI / 3.5))
#define VOXEL_DISTANCE 120
#define VOXEL_MIN_CLEARANCE 10.0f
#define VOXEL_CLEARANCE 30.0f
#define VOXEL_FOLLOW 0.1f // seconds to close most of a step in the ground
#define VOXEL_HEIGHT_SCALE 15.0f
#define VOXEL_MOVE_SPEED 14.0f // map units per second
#define VOXEL_TURN_SPEED 1.2f // radians per second
#define VOXEL_FLY_SPEED 20.0f // height units per second

unsigned char HeightMap[MAP_WIDTH * MAP_HEIGHT];
unsigned char ColorMap[MAP_WIDTH * MAP_HEIGHT];

struct {
	float x;
	float y;
	float height;
	float heading;
	bool flycam;
} Camera = { 0, 0, 0, 0, false };

float BilinearSample(const unsigned char *map, float x, float y)
{
	int ix = floorf(x);
	int iy = floorf(y);
	int u0 = WRAP256(ix);
	int v0 = WRAP256(iy);
	int u1 = WRAP256(ix + 1);
	int v1 = WRAP256(iy + 1);
	float ufraction = x - ix;
	float vfraction = y - iy;

	float top = map[v0 * MAP_WIDTH + u0] + ufraction * (map[v0 * MAP_WIDTH + u1] - map[v0 * MAP_WIDTH + u0]);
	float bottom = map[v1 * MAP_WIDTH + u0] + ufraction * (map[v1 * MAP_WIDTH + u1] - map[v1 * MAP_WIDTH + u0]);
	return top + vfraction * (bottom - top);
}

void DrawVoxelLine(float x0, float y0, float x1, float y1, float cameraheight, float scale, int *lasty, float *lastcolor)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	float sx = (x1 - x0) / RETRO_WIDTH;
	float sy = (y1 - y0) / RETRO_WIDTH;

	for (int x = 0; x < RETRO_WIDTH; x++) {
		float height = BilinearSample(HeightMap, x0, y0);
		float color = BilinearSample(ColorMap, x0, y0);
		int screeny = (int)((height - cameraheight) * scale + (RETRO_HEIGHT / 2));

		if (screeny < lasty[x]) {
			int bottom = lasty[x];
			if (lastcolor[x] == -1) {
				lastcolor[x] = color;
			}

			float colorstep = (color - lastcolor[x]) / (bottom - screeny);
			float currentcolor = lastcolor[x];

			if (bottom > RETRO_HEIGHT - 1) {
				currentcolor += (bottom - (RETRO_HEIGHT - 1)) * colorstep;
				bottom = RETRO_HEIGHT - 1;
			}

			if (screeny < 0) {
				screeny = 0;
			}

			unsigned char *pixel = buffer + bottom * RETRO_WIDTH + x;
			while (screeny < bottom) {
				*pixel = CLAMP256((int)currentcolor);
				currentcolor += colorstep;
				pixel -= RETRO_WIDTH;
				bottom--;
			}

			lasty[x] = screeny;
		}

		lastcolor[x] = color;

		x0 += sx;
		y0 += sy;
	}
}

void DEMO_Update(double deltatime)
{
	float timestep = (float)deltatime;
	float distance = timestep * VOXEL_MOVE_SPEED;
	float rotation = timestep * VOXEL_TURN_SPEED;

	if (RETRO_KeyPressed(SDL_SCANCODE_SPACE)) {
		Camera.flycam = !Camera.flycam;
	}
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) {
		Camera.heading -= rotation;
	}
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) {
		Camera.heading += rotation;
	}
	if (RETRO_KeyState(SDL_SCANCODE_UP)) {
		Camera.x += cosf(Camera.heading) * distance;
		Camera.y += sinf(Camera.heading) * distance;
	}
	if (RETRO_KeyState(SDL_SCANCODE_DOWN)) {
		Camera.x -= cosf(Camera.heading) * distance;
		Camera.y -= sinf(Camera.heading) * distance;
	}
	if (Camera.flycam && RETRO_KeyState(SDL_SCANCODE_R)) {
		Camera.height -= timestep * VOXEL_FLY_SPEED;
	}
	if (Camera.flycam && RETRO_KeyState(SDL_SCANCODE_F)) {
		Camera.height += timestep * VOXEL_FLY_SPEED;
	}

	Camera.x = fmodf(Camera.x, MAP_WIDTH);
	if (Camera.x < 0) {
		Camera.x += MAP_WIDTH;
	}
	Camera.y = fmodf(Camera.y, MAP_HEIGHT);
	if (Camera.y < 0) {
		Camera.y += MAP_HEIGHT;
	}
	Camera.heading = fmodf(Camera.heading, (float)(2 * M_PI));
	if (Camera.heading < 0) {
		Camera.heading += (float)(2 * M_PI);
	}

	if (!Camera.flycam) {
		float ground = BilinearSample(HeightMap, Camera.x, Camera.y);
		float targetheight = ground - VOXEL_CLEARANCE;
		Camera.height += (targetheight - Camera.height) * (1.0f - expf(-timestep / VOXEL_FOLLOW));
		if (Camera.height > ground - VOXEL_MIN_CLEARANCE) {
			Camera.height = ground - VOXEL_MIN_CLEARANCE;
		}
	}
}

void DEMO_Render(double deltatime)
{
	// Clear voxels
	int lasty[RETRO_WIDTH];
	float lastcolor[RETRO_WIDTH];
	for (int i = 0; i < RETRO_WIDTH; i++) {
		lasty[i] = RETRO_HEIGHT;
		lastcolor[i] = -1;
	}

	// Draw voxel
	for (int depth = 0; depth < VOXEL_DISTANCE; depth += 1 + (depth / 64)) {
		float x1 = Camera.x + depth * cosf(Camera.heading - VOXEL_FOV);
		float y1 = Camera.y + depth * sinf(Camera.heading - VOXEL_FOV);
		float x2 = Camera.x + depth * cosf(Camera.heading + VOXEL_FOV);
		float y2 = Camera.y + depth * sinf(Camera.heading + VOXEL_FOV);
		DrawVoxelLine(x1, y1, x2, y2, Camera.height, VOXEL_HEIGHT_SCALE / (depth + 1), lasty, lastcolor);
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_WHITE);

	// Init height map
	for (int p = 256; p > 1; p /= 2) {
		int p2 = p / 2;
		int k = p * 8 + 20;
		int k2 = k / 2;

		for (int y = 0; y < MAP_HEIGHT; y += p) {
			for (int x = 0; x < MAP_WIDTH; x += p) {
				int a = HeightMap[y * MAP_WIDTH + x];
				int b = HeightMap[WRAP256(y + p) * MAP_WIDTH + x];
				int c = HeightMap[y * MAP_WIDTH + WRAP256(x + p)];
				int d = HeightMap[WRAP256(y + p) * MAP_WIDTH + WRAP256(x + p)];

				HeightMap[y * MAP_WIDTH + WRAP256(x + p2)] = CLAMP256(((a + c) / 2) + (RANDOM(k) - k2));
				HeightMap[WRAP256(y + p2) * MAP_WIDTH + WRAP256(x + p2)] = CLAMP256(((a + b + c + d) / 4) + (RANDOM(k) - k2));
				HeightMap[WRAP256(y + p2) * MAP_WIDTH + x] = CLAMP256(((a + b) / 2) + (RANDOM(k) - k2));
			}
		}
	}

	// Smooth height map
	for (int k = 0; k < 5; k++) {
		for (int y = 0; y < MAP_HEIGHT; y++) {
			for (int x = 0; x < MAP_WIDTH; x++) {
				HeightMap[y * MAP_WIDTH + x] = (
					HeightMap[WRAP256(y + 1) * MAP_WIDTH + x] +
					HeightMap[y * MAP_WIDTH + WRAP256(x + 1)] +
					HeightMap[WRAP256(y - 1) * MAP_WIDTH + x] +
					HeightMap[y * MAP_WIDTH + WRAP256(x - 1)]
				) / 4;
			}
		}
	}

	// Init color map
	for (int y = 0; y < MAP_HEIGHT; y++) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			ColorMap[y * MAP_WIDTH + x] = CLAMP256(128 + (HeightMap[WRAP256(y + 1) * MAP_WIDTH + WRAP256(x + 1)] - HeightMap[y * MAP_WIDTH + x]) * 6);
		}
	}

	// Begin at the default clearance instead of flying in from height zero
	Camera.height = BilinearSample(HeightMap, Camera.x, Camera.y) - VOXEL_CLEARANCE;
}
