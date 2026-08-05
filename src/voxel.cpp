//
// Voxel landscape
//
// A wrapping height map, drawn front to back one depth slice at a time. The
// camera looks along (−sin θ, −cos θ). At depth z the 90° frustum meets the
// ground in a segment whose midpoint is z along that heading and whose half
// width is also z (tan 45° = 1):
//
//   left  = z (−cos θ − sin θ,  sin θ − cos θ)
//   right = z ( cos θ − sin θ, −sin θ − cos θ)
//
// Each column samples the map on that segment. Indices are WRAP (floor,
// then into [0, 1024)), not a cast toward zero: (−1, 0) is the last texel,
// not 0. The camera lives on the same torus; θ lives in [0, 2π).
//
// A height difference Δh at depth z is a pinhole
//
//   y = horizon + Δh · FOCAL / z
//
// y grows down, so a peak (Δh < 0) sits above the horizon. Slices are
// painted down to the highest y already filled (hiddeny), so nearer ground
// occludes farther ground. Δz grows with z, so far slices are coarser.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define MAP_HEIGHT 1024
#define MAP_WIDTH 1024
#define MAP_SHIFT 10

#define VOXEL_FOCAL 100.0f // pixels of Δh at z = 1
#define VOXEL_CLEARANCE 10.0f // how far the camera stays above the ground
#define VOXEL_LOD 0.005f // added to Δz each slice, so far samples thin out

struct {
	float x;        // x position on the map
	float y;        // y position on the map
	float height;   // height of the camera
	float angle;    // direction of the camera
	float horizon;  // screen row of the look-level
	float distance; // farthest slice
} camera = { 512, 800, 78, 0, 100, 800 };

int MapOffset(float x, float y)
{
	return (WRAP(y, MAP_HEIGHT) << MAP_SHIFT) + WRAP(x, MAP_WIDTH);
}

void DEMO_Render(double deltatime)
{
	// Move camera
	float speed = deltatime * 60;
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) {
		camera.angle += 0.02f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) {
		camera.angle -= 0.02f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_UP)) {
		camera.x -= (float)sin(camera.angle) * 1.1f * speed;
		camera.y -= (float)cos(camera.angle) * 1.1f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_DOWN)) {
		camera.x += (float)sin(camera.angle) * 0.75f * speed;
		camera.y += (float)cos(camera.angle) * 0.75f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_R)) {
		camera.height += 0.5f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_F)) {
		camera.height -= 0.5f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_A)) {
		camera.horizon += 1.5f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_S)) {
		camera.horizon -= 1.5f * speed;
	}

	camera.x = fmod(camera.x, MAP_WIDTH);
	if (camera.x < 0) {
		camera.x += MAP_WIDTH;
	}
	camera.y = fmod(camera.y, MAP_HEIGHT);
	if (camera.y < 0) {
		camera.y += MAP_HEIGHT;
	}
	camera.angle = fmod(camera.angle, 2 * M_PI);
	if (camera.angle < 0) {
		camera.angle += 2 * M_PI;
	}

	unsigned char *colormap = RETRO_ImageData(0);
	unsigned char *heightmap = RETRO_ImageData(1);
	unsigned char *buffer = RETRO_FrameBuffer();

	// Collision detection
	int cameraoffs = MapOffset(camera.x, camera.y);
	if (heightmap[cameraoffs] + VOXEL_CLEARANCE > camera.height) {
		camera.height = heightmap[cameraoffs] + VOXEL_CLEARANCE;
	}

	float sinang = (float)sin(camera.angle);
	float cosang = (float)cos(camera.angle);

	int hiddeny[RETRO_WIDTH];
	for (int i = 0; i < RETRO_WIDTH; i++) {
		hiddeny[i] = RETRO_HEIGHT;
	}
	float deltaz = 1.0f;

	// Draw from front to back
	for (float z = 1.0f; z < camera.distance; z += deltaz) {
		float plx = -cosang * z - sinang * z;
		float ply = sinang * z - cosang * z;
		float prx = cosang * z - sinang * z;
		float pry = -sinang * z - cosang * z;

		float dx = (prx - plx) / RETRO_WIDTH;
		float dy = (pry - ply) / RETRO_WIDTH;
		plx += camera.x;
		ply += camera.y;
		float invz = VOXEL_FOCAL / z;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int mapoffset = MapOffset(plx, ply);
			int heightonscreen = (int)((camera.height - heightmap[mapoffset]) * invz + camera.horizon);
			if (heightonscreen < 0) {
				heightonscreen = 0;
			}
			unsigned char color = colormap[mapoffset];
			for (int y = heightonscreen; y < hiddeny[x]; y++) {
				buffer[y * RETRO_WIDTH + x] = color;
			}
			if (heightonscreen < hiddeny[x]) {
				hiddeny[x] = heightonscreen;
			}
			plx += dx;
			ply += dy;
		}
		deltaz += VOXEL_LOD;
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/voxel_color_1024x1024.pcx"); // color
	RETRO_LoadImage("assets/voxel_height_1024x1024.pcx"); // height
	RETRO_SetPalette(RETRO_ImagePalette(0));
	RETRO_SetColor(0, 36, 36, 56);
}
