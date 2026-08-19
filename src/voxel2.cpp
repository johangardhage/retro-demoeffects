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
// occludes farther ground. Δz grows with z, so far slices are coarser. The
// camera follows the terrain by default; Space toggles a flycam in which R
// and F move vertically. The arrow keys move and steer; A and S move the
// horizon.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define MAP_HEIGHT 1024
#define MAP_WIDTH 1024
#define MAP_SHIFT 10

#define VOXEL_FOCAL 100.0f // pixels of Δh at z = 1
#define VOXEL_MIN_CLEARANCE 10.0f // closest the camera may come to the ground
#define VOXEL_EYE 49.0f // and how far above it the camera rides
#define VOXEL_FOLLOW 0.10f // seconds the eye takes to close most of a step in the ground
#define VOXEL_FLY_SPEED 30.0f // height units per second
#define VOXEL_LOD 0.005f // added to Δz each slice, so far samples thin out

// The camera rides a set distance above the ground rather than at a set height,
// so walking follows the terrain up as well as down. The eye lags it: the
// ground steps at a cliff edge, and putting the eye there in one frame throws
// the whole view with it.
struct {
	float x;         // x position on the map
	float y;         // y position on the map
	float height;    // height of the eye, lagging the ground it follows
	float angle;     // direction of the camera
	float horizon;   // screen row of the look-level
	float distance;  // farthest slice
	bool flycam;     // free flight instead of terrain following
} Camera = { 512, 800, 0, 0, 100, 800, false };

float BilinearSample(const unsigned char *map, float x, float y)
{
	int ix = floorf(x);
	int iy = floorf(y);
	int u0 = WRAP(ix, MAP_WIDTH);
	int v0 = WRAP(iy, MAP_HEIGHT);
	int u1 = WRAP(ix + 1, MAP_WIDTH);
	int v1 = WRAP(iy + 1, MAP_HEIGHT);
	float ufraction = x - ix;
	float vfraction = y - iy;

	float top = map[(v0 << MAP_SHIFT) + u0] + ufraction * (map[(v0 << MAP_SHIFT) + u1] - map[(v0 << MAP_SHIFT) + u0]);
	float bottom = map[(v1 << MAP_SHIFT) + u0] + ufraction * (map[(v1 << MAP_SHIFT) + u1] - map[(v1 << MAP_SHIFT) + u0]);
	return top + vfraction * (bottom - top);
}

void DEMO_Update(double deltatime)
{
	float timestep = (float)deltatime;
	float speed = timestep * 60;
	if (RETRO_KeyPressed(SDL_SCANCODE_SPACE)) {
		Camera.flycam = !Camera.flycam;
	}
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) {
		Camera.angle += 0.02f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) {
		Camera.angle -= 0.02f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_UP)) {
		Camera.x -= sinf(Camera.angle) * 1.1f * speed;
		Camera.y -= cosf(Camera.angle) * 1.1f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_DOWN)) {
		Camera.x += sinf(Camera.angle) * 0.75f * speed;
		Camera.y += cosf(Camera.angle) * 0.75f * speed;
	}
	if (Camera.flycam && RETRO_KeyState(SDL_SCANCODE_R)) {
		Camera.height += timestep * VOXEL_FLY_SPEED;
	}
	if (Camera.flycam && RETRO_KeyState(SDL_SCANCODE_F)) {
		Camera.height -= timestep * VOXEL_FLY_SPEED;
	}
	if (RETRO_KeyState(SDL_SCANCODE_A)) {
		Camera.horizon += 1.5f * speed;
	}
	if (RETRO_KeyState(SDL_SCANCODE_S)) {
		Camera.horizon -= 1.5f * speed;
	}

	Camera.x = fmodf(Camera.x, MAP_WIDTH);
	if (Camera.x < 0) {
		Camera.x += MAP_WIDTH;
	}
	Camera.y = fmodf(Camera.y, MAP_HEIGHT);
	if (Camera.y < 0) {
		Camera.y += MAP_HEIGHT;
	}
	Camera.angle = fmodf(Camera.angle, (float)(2 * M_PI));
	if (Camera.angle < 0) {
		Camera.angle += (float)(2 * M_PI);
	}

	unsigned char *heightmap = RETRO_ImageData(1);

	// Follow the terrain, both up and down, closing a fixed fraction of the gap
	// per second. The floor still holds, so lagging cannot leave the eye inside
	// the ground.
	if (!Camera.flycam) {
		float ground = BilinearSample(heightmap, Camera.x, Camera.y);
		Camera.height += (ground + VOXEL_EYE - Camera.height) * (1.0f - expf(-timestep / VOXEL_FOLLOW));
		if (Camera.height < ground + VOXEL_MIN_CLEARANCE) {
			Camera.height = ground + VOXEL_MIN_CLEARANCE;
		}
	}
}

void DEMO_Render(double deltatime)
{
	unsigned char *colormap = RETRO_ImageData(0);
	unsigned char *heightmap = RETRO_ImageData(1);
	unsigned char *buffer = RETRO_FrameBuffer();

	float sinang = sinf(Camera.angle);
	float cosang = cosf(Camera.angle);

	int hiddeny[RETRO_WIDTH];
	for (int i = 0; i < RETRO_WIDTH; i++) {
		hiddeny[i] = RETRO_HEIGHT;
	}
	float deltaz = 1.0f;

	// Draw from front to back
	for (float z = 1.0f; z < Camera.distance; z += deltaz) {
		float plx = -cosang * z - sinang * z;
		float ply = sinang * z - cosang * z;
		float prx = cosang * z - sinang * z;
		float pry = -sinang * z - cosang * z;

		float dx = (prx - plx) / RETRO_WIDTH;
		float dy = (pry - ply) / RETRO_WIDTH;
		plx += Camera.x;
		ply += Camera.y;
		float invz = VOXEL_FOCAL / z;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int mapoffset = (WRAP(ply, MAP_HEIGHT) << MAP_SHIFT) + WRAP(plx, MAP_WIDTH);
			int heightonscreen = (int)((Camera.height - heightmap[mapoffset]) * invz + Camera.horizon);
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
	RETRO_LoadImage("assets/voxel_color_1024x1024.pcx", true); // color
	RETRO_LoadImage("assets/voxel_height_1024x1024.pcx"); // height
	RETRO_SetColor(0, 36, 36, 56);

	// Start on the ground rather than lagging up to it
	Camera.height = BilinearSample(RETRO_ImageData(1), Camera.x, Camera.y) + VOXEL_EYE;
}
