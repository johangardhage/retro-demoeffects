//
// voxel.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define MAP_HEIGHT 1024
#define MAP_WIDTH 1024
#define MAP_SHIFT 10

struct {
	float x;        // x position on the map
	float y;        // y position on the map
	float height;   // height of the camera
	float angle;    // direction of the camera
	float horizon;  // horizon position (look up and down)
	float distance; // distance of map
} camera = { 512, 800, 78, 0, 100, 800 };

void DEMO_Render(double deltatime)
{
	if (RETRO_KeyState(SDLK_LEFT)) {
		camera.angle += 0.02f;
	}
	if (RETRO_KeyState(SDLK_RIGHT)) {
		camera.angle -= 0.02f;
	}
	if (RETRO_KeyState(SDLK_UP)) {
		camera.x -= (float)sin(camera.angle) * 1.1f;
		camera.y -= (float)cos(camera.angle) * 1.1f;
	}
	if (RETRO_KeyState(SDLK_DOWN)) {
		camera.x += (float)sin(camera.angle) * 0.75f;
		camera.y += (float)cos(camera.angle) * 0.75f;
	}
	if (RETRO_KeyState(SDLK_r)) {
		camera.height += 0.5f;
	}
	if (RETRO_KeyState(SDLK_f)) {
		camera.height -= 0.5f;
	}
	if (RETRO_KeyState(SDLK_a)) {
		camera.horizon += 1.5f;
	}
	if (RETRO_KeyState(SDLK_s)) {
		camera.horizon -= 1.5f;
	}

	unsigned char *colormap = RETRO_ImageData(0);
	unsigned char *heightmap = RETRO_ImageData(1);
	unsigned char *buffer = RETRO_FrameBuffer();

	int mapwidthperiod = MAP_WIDTH - 1;
	int mapheightperiod = MAP_HEIGHT - 1;

	// Collision detection
	int cameraoffs = ((((int)camera.y) & mapwidthperiod) << MAP_SHIFT) + (((int)camera.x) & mapheightperiod);
	if ((heightmap[cameraoffs] + 10.0f) > camera.height) {
		camera.height = heightmap[cameraoffs] + 10.0f;
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
		// 90 degree field of view
		float plx = -cosang * z - sinang * z;
		float ply = sinang * z - cosang * z;
		float prx = cosang * z - sinang * z;
		float pry = -sinang * z - cosang * z;

		float dx = (prx - plx) / RETRO_WIDTH;
		float dy = (pry - ply) / RETRO_WIDTH;
		plx += camera.x;
		ply += camera.y;
		float invz = 1.0f / z * 100.0f;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int mapoffset = ((((int)ply) & mapwidthperiod) << MAP_SHIFT) + (((int)plx) & mapheightperiod);
			int heightonscreen = (int)((camera.height - heightmap[mapoffset]) * invz + camera.horizon);
			if (heightonscreen < 0) {
				heightonscreen = 0;
			}
			char color = colormap[mapoffset];
			for (int y = heightonscreen; y < hiddeny[x]; y++) {
				buffer[y * RETRO_WIDTH + x] = color;
			}
			if (heightonscreen < hiddeny[x]) {
				hiddeny[x] = heightonscreen;
			}
			plx += dx;
			ply += dy;
		}
		deltaz += 0.005f;
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/voxel_color_1024x1024.pcx"); // color
	RETRO_LoadImage("assets/voxel_height_1024x1024.pcx"); // height
	RETRO_SetPalette(RETRO_ImagePalette(0));
	RETRO_SetColor(0, 36, 36, 56);
}
