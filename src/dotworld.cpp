//
// Dot world
//
// A procedurally generated height map rendered as a field of dots.
// Several octaves of smooth value noise form the terrain; a radial falloff
// turns its edges into sea so the landscape resembles a small island world.
// The arrow keys move the camera: Left and Right turn, Up and Down move.
// R raises the camera and F lowers it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define MAP_SIZE 128
#define WORLD_HEIGHT_SCALE 0.34f
#define WORLD_MOVE_SPEED 24.0f
#define WORLD_TURN_SPEED 1.35f
#define WORLD_FLY_SPEED 28.0f
#define WORLD_NEAR_PLANE 4.0f
#define WORLD_MIN_HEIGHT 4.0f
#define WORLD_MAX_HEIGHT 120.0f

// HeightMap stores terrain altitude. ColorMap stores the palette index of the
// corresponding dot, so neither value has to be recalculated while rendering.
unsigned char HeightMap[MAP_SIZE * MAP_SIZE];
unsigned char ColorMap[MAP_SIZE * MAP_SIZE];

// More than one terrain dot may land on the same screen pixel. The depth
// buffer ensures that the nearest one remains visible.
unsigned int DotWorldZBuffer[RETRO_WIDTH * RETRO_HEIGHT];

// Camera coordinates use the same X/Z plane as the height map. Height is the
// vertical Y coordinate, and heading is measured in radians around Y.
struct {
	float x;
	float z;
	float height;
	float heading;
} Camera = { MAP_SIZE / 2.0f, -35.0f, 46.0f, 0.0f };

// Procedural noise

// Hash an integer grid coordinate into a deterministic pseudo-random value.
// Unlike rand(), this always returns the same value for the same (x, y), which
// makes terrain generation repeatable between runs.
static unsigned int Hash(int x, int y)
{
	unsigned int n = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
	n = (n ^ (n >> 13)) * 1274126177u;
	return n ^ (n >> 16);
}

// Remap t from 0..1 through a cubic smoothstep curve. This keeps the value at
// both ends unchanged while flattening the slope at cell boundaries.
static float Smooth(float t)
{
	// Cubic smoothstep gives adjacent noise cells a continuous slope.
	return t * t * (3.0f - 2.0f * t);
}

// Sample deterministic 2D value noise at floating-point coordinates. The
// result is bilinearly interpolated from the four surrounding hashed lattice
// values and lies in the range 0..1.
static float ValueNoise(float x, float y)
{
	// Find the four integer lattice points surrounding the sample.
	int ix = (int)floor(x);
	int iy = (int)floor(y);
	float fx = Smooth(x - ix);
	float fy = Smooth(y - iy);
	float a = (Hash(ix, iy) & 65535) / 65535.0f;
	float b = (Hash(ix + 1, iy) & 65535) / 65535.0f;
	float c = (Hash(ix, iy + 1) & 65535) / 65535.0f;
	float d = (Hash(ix + 1, iy + 1) & 65535) / 65535.0f;

	// Bilinearly interpolate their values using the smoothed fractions.
	float top = a + (b - a) * fx;
	float bottom = c + (d - c) * fx;
	return top + (bottom - top) * fy;
}

// Generate the complete height and color maps. Five noise octaves produce the
// terrain, a radial falloff shapes it into an island, and altitude plus local
// slope determine the palette index assigned to every point.
static void GenerateWorld(void)
{
	// Build fractal noise by combining broad, strong hills with progressively
	// finer and weaker detail. Dividing by total normalizes the result to 0..1.
	for (int z = 0; z < MAP_SIZE; z++) {
		for (int x = 0; x < MAP_SIZE; x++) {
			float noise = 0.0f;
			float amplitude = 1.0f;
			float total = 0.0f;
			float frequency = 1.0f / 48.0f;

			for (int octave = 0; octave < 5; octave++) {
				noise += ValueNoise(x * frequency + 11.7f,
					z * frequency + 23.1f) * amplitude;
				total += amplitude;
				frequency *= 2.0f;
				amplitude *= 0.5f;
			}

			// Lower samples toward every map edge to form an island instead of a
			// square slab. Anything below sea level becomes flat water.
			float dx = (x - MAP_SIZE / 2.0f) / (MAP_SIZE / 2.0f);
			float dz = (z - MAP_SIZE / 2.0f) / (MAP_SIZE / 2.0f);
			float island = 1.0f - (dx * dx + dz * dz) * 0.72f;
			float h = (noise / total * island - 0.25f) * 255.0f;
			if (h < 22.0f) h = 22.0f;
			if (h > 255.0f) h = 255.0f;
			HeightMap[z * MAP_SIZE + x] = (unsigned char)h;
		}
	}

	// Assign water, beach, vegetation and rock palette bands by altitude. The
	// local height differences add simple directional lighting to the slopes.
	for (int z = 0; z < MAP_SIZE; z++) {
		for (int x = 0; x < MAP_SIZE; x++) {
			int i = z * MAP_SIZE + x;
			int h = HeightMap[i];
			int left = HeightMap[z * MAP_SIZE + (x > 0 ? x - 1 : x)];
			int back = HeightMap[(z > 0 ? z - 1 : z) * MAP_SIZE + x];
			int light = (h - left + h - back) / 5;
			int color;
			if (h <= 24) color = 16 + (x + z) % 5;       // water
			else if (h < 58) color = 32 + (h - 25) / 5; // shore
			else if (h < 145) color = 48 + (h - 58) / 5 + light;
			else color = 72 + (h - 145) / 7 + light;
			if (color < 1) color = 1;
			if (color > 95) color = 95;
			ColorMap[i] = (unsigned char)color;
		}
	}
}

// Advance camera state by one fixed simulation step. Arrow keys steer and move
// over the X/Z plane, while R and F raise and lower the camera on the Y axis.
// deltatime is measured in seconds.
void DEMO_Update(double deltatime)
{
	// Speeds are expressed per second, keeping controls consistent at any FPS.
	float distance = (float)deltatime * WORLD_MOVE_SPEED;
	float rotation = (float)deltatime * WORLD_TURN_SPEED;

	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) Camera.heading -= rotation;
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) Camera.heading += rotation;
	if (RETRO_KeyState(SDL_SCANCODE_UP)) {
		Camera.x += sinf(Camera.heading) * distance;
		Camera.z += cosf(Camera.heading) * distance;
	}
	if (RETRO_KeyState(SDL_SCANCODE_DOWN)) {
		Camera.x -= sinf(Camera.heading) * distance;
		Camera.z -= cosf(Camera.heading) * distance;
	}
	if (RETRO_KeyState(SDL_SCANCODE_R)) {
		Camera.height += (float)deltatime * WORLD_FLY_SPEED;
	}
	if (RETRO_KeyState(SDL_SCANCODE_F)) {
		Camera.height -= (float)deltatime * WORLD_FLY_SPEED;
	}

	// Allow the camera to travel beyond the shore, but keep the finite world
	// close enough that the player cannot permanently lose sight of it.
	Camera.x = fmaxf(-MAP_SIZE / 2.0f, fminf(Camera.x, MAP_SIZE * 1.5f));
	Camera.z = fmaxf(-MAP_SIZE / 2.0f, fminf(Camera.z, MAP_SIZE * 1.5f));
	Camera.height = fmaxf(WORLD_MIN_HEIGHT, fminf(Camera.height, WORLD_MAX_HEIGHT));
	Camera.heading = fmodf(Camera.heading, (float)(2.0 * M_PI));
}

// Render the height map as a perspective-projected point cloud. Each terrain
// sample becomes one screen-space dot, with a z-buffer resolving overlapping
// samples. Rendering is entirely derived from the current Camera state.
void DEMO_Render(double deltatime)
{
	(void)deltatime;
	float cosa = cosf(Camera.heading);
	float sina = sinf(Camera.heading);
	float horizon = RETRO_HEIGHT * 0.46f;

	memset(DotWorldZBuffer, 0xFF, sizeof(DotWorldZBuffer));

	for (int z = 0; z < MAP_SIZE; z++) {
		for (int x = 0; x < MAP_SIZE; x++) {
			int mapindex = z * MAP_SIZE + x;
			// Translate the world relative to the camera, then rotate it into
			// camera space. xt is horizontal and zt is depth into the screen.
			float wx = x - Camera.x;
			float wz = z - Camera.z;
			float xt = wx * cosa - wz * sina;
			float zt = wx * sina + wz * cosa;
			float yt = HeightMap[mapindex] * WORLD_HEIGHT_SCALE - Camera.height;

			// Reject points behind or extremely close to the camera before the
			// perspective divide. Project the remaining 3D points to the screen.
			if (zt <= WORLD_NEAR_PLANE) continue;
			int sx = (int)(RETRO_WIDTH / 2.0f + RETRO_WIDTH * 0.54f * xt / zt);
			int sy = (int)(horizon - RETRO_HEIGHT * 0.78f * yt / zt);
			if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) continue;

			// Quantize depth to fixed point and draw only the closest dot at this
			// pixel. This makes the result independent of map traversal order.
			unsigned int depth = (unsigned int)(zt * 256.0f);
			int screenindex = sy * RETRO_WIDTH + sx;
			if (depth < DotWorldZBuffer[screenindex]) {
				DotWorldZBuffer[screenindex] = depth;
				RETRO_PutPixel(sx, sy, ColorMap[mapindex]);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 16, RETRO_BLACK, RETRO_BLUEBLACK);
	RETRO_CreateGradientPalette(16, 32, RETRO_DARKBLUE, RETRO_CERULEAN);
	RETRO_CreateGradientPalette(32, 48, RETRO_SADDLEBROWN, RETRO_JASMINE);
	RETRO_CreateGradientPalette(48, 72, RETRO_HUNTERGREEN, RETRO_MOSSGREEN);
	RETRO_CreateGradientPalette(72, 96, RETRO_SAGE, RETRO_WHITE);
	GenerateWorld();
}
