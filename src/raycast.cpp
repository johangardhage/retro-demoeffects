//
// Raycast
//
// A small maze of textured rooms, drawn one column at a time by casting a ray
// per column across a grid of walls, with textured floors and ceilings.
//
// The camera stands at p looking along d, with the screen a plane across the
// view whose half-width is c = tan(RAYCAST_FOV / 2), perpendicular to d.
// Column x casts the ray r = d + (2x / W − 1) c and walks it across the grid
// cell by cell (a DDA). For each axis it keeps the ray parameter t at the
// next grid line it will cross, steps over whichever line is nearer, and
// stops at the first wall cell it enters. c is perpendicular to d, so r · d
// is 1 and the t of the last line crossed is the wall's distance z along d,
// not along the ray. That keeps straight walls straight, where the ray's own
// length would bow them. A wall of height 1 at z covers F / z rows, with
// F = W / (2 tan) so that the pixels come out square, centered on the
// horizon with the eye halfway up. Where the ray hits the wall gives the
// texture's column, and the row runs down it evenly.
//
// A floor row y below the horizon sees the floor at
//
//   z = F / (2 (y − horizon))
//
// the same for every column, so the floor under column x is p + z r and a
// texel of the floor's is read there. The ceiling is the same above the
// horizon.
//
// The maze is three maps of MAP_SIZE × MAP_SIZE cells, indexed [x][y]: the
// walls, the floor and the ceiling each name a tile of the texture sheet,
// which holds TILE_SIZE-square tiles, SHEET_TILES to a row. A wall's tile is
// its map value less one, 0 being an open cell. A wall's texture is turned
// so that it never reads mirrored.
//
// The arrow keys move the camera: up and down walk, left and right turn. The
// camera keeps a square of CAMERA_RADIUS clear of every wall, and a step
// that would enter one moves along each axis on its own, so the camera
// slides along a wall it walks into rather than stopping dead.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrovector.h"

#define MAP_SIZE 16
#define TILE_SIZE 64 // texels across a tile
#define SHEET_TILES 5 // tiles across a row of a sheet
#define SHEET_WIDTH (SHEET_TILES * TILE_SIZE)
#define RAYCAST_FOV 66.0 // degrees across the screen
#define WALK_SPEED 1.5 // cells a second, under the arrow keys
#define TURN_SPEED 2.0 // radians a second, under the arrow keys
#define CAMERA_RADIUS 0.25 // cells kept clear round the camera

static const int Walls[MAP_SIZE][MAP_SIZE] = {
	{ 2, 2, 2, 2, 7, 4, 7, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 9, 2, 0, 2, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2 },
	{ 9, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 2 },
	{ 9, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 2 },
	{ 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 2 },
	{ 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 2 },
	{ 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9, 0, 0, 2 },
	{ 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 },
	{ 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 9, 9, 0, 0, 2 },
	{ 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 2 },
	{ 2, 2, 2, 0, 0, 0, 2, 2, 0, 2, 0, 0, 0, 0, 0, 2 },
	{ 7, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 2 },
	{ 7, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2 },
	{ 7, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 2 },
	{ 7, 7, 7, 7, 7, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
};

static const int Floors[MAP_SIZE][MAP_SIZE] = {
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 5, 5, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 5, 5, 5, 5, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 5, 5, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
	{ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
};

static const int Ceilings[MAP_SIZE][MAP_SIZE] = {
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
	{ 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 },
};

static unsigned char *Textures; // the texture sheet

//
// A texel of a tile of a sheet
//
static unsigned char SheetTexel(const unsigned char *sheet, int tile, int u, int v)
{
	return sheet[(tile / SHEET_TILES * TILE_SIZE + v) * SHEET_WIDTH + tile % SHEET_TILES * TILE_SIZE + u];
}

//
// Whether a camera at (x, y) would come within CAMERA_RADIUS of a wall
//
static bool Blocked(float x, float y)
{
	for (int corner = 0; corner < 4; corner++) {
		int cellx = (int)floorf(x + (corner & 1 ? CAMERA_RADIUS : -CAMERA_RADIUS));
		int celly = (int)floorf(y + (corner & 2 ? CAMERA_RADIUS : -CAMERA_RADIUS));
		if (Walls[cellx][celly] != 0) {
			return true;
		}
	}
	return false;
}

void DEMO_Render(double time, double deltatime)
{
	static vec2 position = { 6.5f, 5.5f }; // the camera, in cells
	static float angle = (float)M_PI; // the way it looks, from x toward y

	unsigned char *buffer = RETRO_FrameBuffer();

	// Move the camera under the arrow keys
	bool left = RETRO_KeyState(SDL_SCANCODE_LEFT), right = RETRO_KeyState(SDL_SCANCODE_RIGHT);
	bool up = RETRO_KeyState(SDL_SCANCODE_UP), down = RETRO_KeyState(SDL_SCANCODE_DOWN);
	angle = (float)fmod(angle + ((right ? 1 : 0) - (left ? 1 : 0)) * TURN_SPEED * deltatime, 2 * M_PI);
	float step = (float)(((up ? 1 : 0) - (down ? 1 : 0)) * WALK_SPEED * deltatime);
	float nextx = position.x + step * cosf(angle), nexty = position.y + step * sinf(angle);
	if (!Blocked(nextx, position.y)) {
		position.x = nextx;
	}
	if (!Blocked(position.x, nexty)) {
		position.y = nexty;
	}

	float halfwidth = (float)tan(radians(RAYCAST_FOV / 2.0));
	float dirx = cosf(angle), diry = sinf(angle);
	float planex = -diry * halfwidth, planey = dirx * halfwidth;
	float focal = RETRO_WIDTH / (2 * halfwidth);
	float horizon = RETRO_HEIGHT / 2.0f;

	// Find the distance to the floor or the ceiling on every row
	float rowdistance[RETRO_HEIGHT];
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		rowdistance[y] = focal / (2 * fabsf(y + 0.5f - horizon));
	}

	for (int x = 0; x < RETRO_WIDTH; x++) {
		// Walk the column's ray across the grid to the first wall
		float camera = 2.0f * (x + 0.5f) / RETRO_WIDTH - 1;
		float rayx = dirx + planex * camera, rayy = diry + planey * camera;
		int cellx = (int)floorf(position.x), celly = (int)floorf(position.y);
		float deltax = rayx != 0 ? fabsf(1 / rayx) : 1e30f;
		float deltay = rayy != 0 ? fabsf(1 / rayy) : 1e30f;
		int stepx = rayx < 0 ? -1 : 1, stepy = rayy < 0 ? -1 : 1;
		float sidex = (rayx < 0 ? position.x - cellx : cellx + 1 - position.x) * deltax;
		float sidey = (rayy < 0 ? position.y - celly : celly + 1 - position.y) * deltay;
		bool alongx;
		float z;
		for (;;) {
			if (sidex < sidey) {
				z = sidex;
				sidex += deltax;
				cellx += stepx;
				alongx = true;
			} else {
				z = sidey;
				sidey += deltay;
				celly += stepy;
				alongx = false;
			}
			if (Walls[cellx][celly] != 0) {
				break;
			}
		}

		// Find the texture's column where the ray met the wall, turned so no wall reads mirrored
		float hit = alongx ? position.y + z * rayy : position.x + z * rayx;
		int u = CLAMP(fract(hit) * TILE_SIZE, 0, TILE_SIZE);
		if ((alongx && rayx < 0) || (!alongx && rayy > 0)) {
			u = TILE_SIZE - 1 - u;
		}
		int walltile = Walls[cellx][celly] - 1;
		float height = focal / z;
		float top = horizon - height / 2;
		float bottom = horizon + height / 2;

		unsigned char *pixel = buffer + x;
		for (int y = 0; y < RETRO_HEIGHT; y++, pixel += RETRO_WIDTH) {
			if (y + 0.5f >= top && y + 0.5f < bottom) {
				int v = CLAMP((y + 0.5f - top) / height * TILE_SIZE, 0, TILE_SIZE);
				*pixel = SheetTexel(Textures, walltile, u, v);
				continue;
			}

			// Floor below the wall, ceiling above, the texel's row along y and its column along x
			float distance = rowdistance[y];
			float floorx = position.x + distance * rayx, floory = position.y + distance * rayy;
			int mapx = (int)floorf(floorx), mapy = (int)floorf(floory);
			int tx = CLAMP((floorx - mapx) * TILE_SIZE, 0, TILE_SIZE);
			int ty = CLAMP((floory - mapy) * TILE_SIZE, 0, TILE_SIZE);
			bool below = y + 0.5f >= bottom;
			int tile = below ? Floors[mapx][mapy] : Ceilings[mapx][mapy];
			*pixel = SheetTexel(Textures, tile, tx, ty);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *textures = RETRO_LoadImage("assets/raycast_walls_320x200.pcx", true);
	if (textures->width != SHEET_WIDTH) {
		RETRO_RageQuit("Raycast sheet must be %d wide\n", SHEET_WIDTH);
	}
	Textures = textures->data;
}
