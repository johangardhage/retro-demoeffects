//
// Raycast, with light maps and doors
//
// A small maze of textured rooms, drawn one column at a time by casting a ray
// per column across a grid of walls, with textured floors and ceilings, the
// light dying away with distance and pools of light laid over it by light
// maps.
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
// F = W / (2 tan) so that the pixels come out square, and the eye sits at
// the height e, so the wall runs from the horizon − (1 − e) F / z to the
// horizon + e F / z. Where the ray hits the wall gives the texture's column,
// and the row runs down it evenly.
//
// A floor row y below the horizon sees the floor at
//
//   z = e F / (y − horizon)
//
// the same for every column, so the floor under column x is p + z r and a
// texel of the floor's is read there. The ceiling is the same above the
// horizon at the height 1 − e. The eye bobs a little up and down with the
// walk.
//
// The maze is five maps of MAP_SIZE × MAP_SIZE cells, indexed [x][y]: the
// walls, the floor and the ceiling each name a tile of the texture sheet,
// and the floor lights and ceiling lights each name a tile of the light
// sheet. Both sheets hold TILE_SIZE-square tiles, SHEET_TILES to a row. A
// wall's tile is its map value less one, 0 being an open cell, and a wall
// takes its light from the floor light map at its own cell. That light is
// cast from the lit floor beside it, so it falls only on the faces that
// look into lit floor cells, and a wall lit from one room is dark from the
// next. A wall none of whose faces look into lit floor carries its own
// light, a lamp set in it, and shows it on every face. A light tile
// runs along the grid, as the floor's do, so a pool that spans several
// cells joins up across them, on a wall as on the floor; a wall's texture
// is turned instead so that it never reads mirrored.
//
// The light at a texel is
//
//   level = LIGHT_NEAR min(1, LIGHT_REACH / z) + LIGHT_AMBIENT + light tile
//
// up to MAXLIGHT, where the light tile's texel is a level of its own. Near
// light and ambient together stay well short of MAXLIGHT, so a pool of the
// light maps reads brighter than the floor round it at every distance, right
// under the camera as well as far off. Walls along x take the distance term
// at RAYCAST_SIDE of its strength, so corners read. The palette is the
// texture sheet's own, and LightTable holds every entry of it at every
// level, matched back into it.
//
// A door stands across its cell halfway along y, between walls on either
// side of it in x, and is drawn with DOOR_TILE. A ray that crosses the
// door's line inside the cell, y = celly + 1/2, stops there where it meets
// the door and passes on where the door has slid away, so the door is a wall
// set back half a cell, and lit as one, with a gap that widens as it opens.
// The floor of its cell is lit on either side of it only where the floor
// beyond is lit, so light does not pass under the door, while a lamp in the
// ceiling of its cell hangs over the door and lights both sides. It opens by
// sliding along x into the wall past it, its texture going with it, while
// the camera is within DOOR_RANGE of its middle, and closes again once the
// camera has gone. It blocks the camera until it is DOOR_PASSABLE open.
//
// The camera's position, in cells, is printed in the top left corner.
//
// The arrow keys move the camera: up and down walk, left and right turn. The
// camera keeps a square of CAMERA_RADIUS clear of every wall, and a step
// that would enter one moves along each axis on its own, so the camera
// slides along a wall it walks into rather than stopping dead. The bob
// follows the distance walked.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"
#include "lib/retrovector.h"

#define MAP_SIZE 16
#define TILE_SIZE 64 // texels across a tile
#define SHEET_TILES 5 // tiles across a row of a sheet
#define SHEET_WIDTH (SHEET_TILES * TILE_SIZE)
#define RAYCAST_FOV 66.0 // degrees across the screen
#define RAYCAST_SIDE 0.7 // distance light on walls along x
#define MAXLIGHT 32 // levels of light above none
#define LIGHTS (MAXLIGHT + 1)
#define LIGHT_NEAR 16 // levels of distance light up close
#define LIGHT_REACH 1.5 // cells within which the distance light is full
#define LIGHT_AMBIENT 6 // levels everywhere
#define WALK_SPEED 1.5 // cells a second, under the arrow keys
#define TURN_SPEED 2.0 // radians a second, under the arrow keys
#define CAMERA_RADIUS 0.25 // cells kept clear round the camera
#define DOOR_TILE 3 // the skull panel
#define DOOR_RANGE 2.0 // cells from its middle within which a door opens
#define DOOR_SPEED 2.0 // of its width a second, opening or closing
#define DOOR_PASSABLE 0.9 // of its width open before the camera can pass
#define TEXT_X 4 // pixels, where the position is printed
#define TEXT_Y 4
#define BOB_HEIGHT 0.012 // of a wall, either way
#define BOB_STRIDE 1.4 // cells a step

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

static const int FloorLights[MAP_SIZE][MAP_SIZE] = {
	{ 0, 0, 0, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 7, 0 },
	{ 0, 0, 0, 0, 2, 7, 12, 0, 0, 0, 0, 0, 0, 0, 4, 7 },
	{ 0, 0, 0, 0, 3, 8, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 7, 0, 0, 0 },
	{ 0, 0, 0, 0, 1, 6, 11, 0, 0, 0, 0, 7, 7, 7, 0, 0 },
	{ 0, 0, 0, 0, 2, 7, 12, 0, 0, 0, 0, 7, 7, 0, 0, 0 },
	{ 0, 0, 0, 0, 3, 8, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0 },
};

static const int CeilingLights[MAP_SIZE][MAP_SIZE] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

struct Door {
	int x, y; // its cell, open, between walls at x − 1 and x + 1
	float open; // of its width slid away into the wall at x + 1
};

static Door Doors[] = {
	{ 7, 12, 0 },
};

static unsigned char *Textures; // the texture sheet
static unsigned char *LightMaps; // the light sheet, a level at every texel
static unsigned char LightTable[RETRO_COLORS * LIGHTS];
static unsigned char TextColor, ShadowColor; // the palette's nearest to white and to black

//
// A texel of a tile of a sheet
//
static unsigned char SheetTexel(const unsigned char *sheet, int tile, int u, int v)
{
	return sheet[(tile / SHEET_TILES * TILE_SIZE + v) * SHEET_WIDTH + tile % SHEET_TILES * TILE_SIZE + u];
}

//
// A texel lit by the distance light and the light tile's own level
//
static unsigned char Lit(unsigned char texel, float distancelight, int tilelight)
{
	return LightTable[texel * LIGHTS + CLAMP(distancelight + LIGHT_AMBIENT + tilelight, 0, LIGHTS)];
}

//
// The door in cell (x, y), or none
//
static Door *DoorAt(int x, int y)
{
	for (Door &door : Doors) {
		if (door.x == x && door.y == y) {
			return &door;
		}
	}
	return NULL;
}

//
// The floor light tile at open cell (x, y), at fy of the way across it in
// y. A door's cell is split by the door, and the floor on either side of it
// takes the cell's light only where the floor it opens onto on that side is
// lit, as a wall's face does, so light does not pass under the door
//
static int FloorLight(int x, int y, float fy)
{
	if (DoorAt(x, y) && FloorLights[x][fy < 0.5f ? y - 1 : y + 1] == 0) {
		return 0;
	}
	return FloorLights[x][y];
}

//
// Whether a camera at (x, y) would come within CAMERA_RADIUS of a wall, or of
// the cell of a door not yet open enough to pass
//
static bool Blocked(float x, float y)
{
	for (int corner = 0; corner < 4; corner++) {
		int cellx = (int)floorf(x + (corner & 1 ? CAMERA_RADIUS : -CAMERA_RADIUS));
		int celly = (int)floorf(y + (corner & 2 ? CAMERA_RADIUS : -CAMERA_RADIUS));
		Door *door = DoorAt(cellx, celly);
		if (Walls[cellx][celly] != 0 || (door && door->open < DOOR_PASSABLE)) {
			return true;
		}
	}
	return false;
}

//
// The light tile on the face of wall cell (x, y) that looks into open cell
// (frontx, fronty): the wall's own, where the floor it looks onto is lit or
// where no face of the wall looks onto lit floor, and none otherwise
//
static int WallLight(int x, int y, int frontx, int fronty)
{
	if (FloorLights[frontx][fronty] != 0) {
		return FloorLights[x][y];
	}
	static const int sides[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
	for (const int *side : sides) {
		int nx = x + side[0], ny = y + side[1];
		if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE && Walls[nx][ny] == 0 && FloorLights[nx][ny] != 0) {
			return 0;
		}
	}
	return FloorLights[x][y];
}

void DEMO_Render(RETRO_Time time)
{
	static vec2 position = { 6.5f, 5.5f }; // the camera, in cells
	static float angle = (float)M_PI; // the way it looks, from x toward y
	static double walked; // cells walked, for the bob

	unsigned char *buffer = RETRO_FrameBuffer();

	// Move the camera under the arrow keys
	bool left = RETRO_KeyState(SDL_SCANCODE_LEFT), right = RETRO_KeyState(SDL_SCANCODE_RIGHT);
	bool up = RETRO_KeyState(SDL_SCANCODE_UP), down = RETRO_KeyState(SDL_SCANCODE_DOWN);
	angle = (float)fmod(angle + ((right ? 1 : 0) - (left ? 1 : 0)) * TURN_SPEED * time.delta, 2 * M_PI);
	float step = (float)(((up ? 1 : 0) - (down ? 1 : 0)) * WALK_SPEED * time.delta);
	float nextx = position.x + step * cosf(angle), nexty = position.y + step * sinf(angle);
	if (!Blocked(nextx, position.y)) {
		position.x = nextx;
	}
	if (!Blocked(position.x, nexty)) {
		position.y = nexty;
	}
	walked += fabsf(step);
	double phase = fmod(walked / BOB_STRIDE * 2 * M_PI, 2 * M_PI);

	// Open the doors the camera is near, close the others
	for (Door &door : Doors) {
		float dx = position.x - (door.x + 0.5f), dy = position.y - (door.y + 0.5f);
		bool near = dx * dx + dy * dy < DOOR_RANGE * DOOR_RANGE;
		door.open = CLAMP01(door.open + (float)((near ? 1 : -1) * DOOR_SPEED * time.delta));
	}
	float halfwidth = (float)tan(radians(RAYCAST_FOV / 2.0));
	float dirx = cosf(angle), diry = sinf(angle);
	float planex = -diry * halfwidth, planey = dirx * halfwidth;
	float focal = RETRO_WIDTH / (2 * halfwidth);
	float eye = 0.5f + BOB_HEIGHT * sinf((float)phase);
	float horizon = RETRO_HEIGHT / 2.0f;

	// Find the distance to the floor or the ceiling on every row
	float rowdistance[RETRO_HEIGHT];
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float rows = y + 0.5f - horizon;
		rowdistance[y] = rows > 0 ? eye * focal / rows : (1 - eye) * focal / -rows;
	}

	for (int x = 0; x < RETRO_WIDTH; x++) {
		// Walk the column's ray across the grid to the first wall or closed part of a door
		float camera = 2.0f * (x + 0.5f) / RETRO_WIDTH - 1;
		float rayx = dirx + planex * camera, rayy = diry + planey * camera;
		int cellx = (int)floorf(position.x), celly = (int)floorf(position.y);
		float deltax = rayx != 0 ? fabsf(1 / rayx) : 1e30f;
		float deltay = rayy != 0 ? fabsf(1 / rayy) : 1e30f;
		int stepx = rayx < 0 ? -1 : 1, stepy = rayy < 0 ? -1 : 1;
		float sidex = (rayx < 0 ? position.x - cellx : cellx + 1 - position.x) * deltax;
		float sidey = (rayy < 0 ? position.y - celly : celly + 1 - position.y) * deltay;
		bool alongx = true;
		float z = 0; // the ray parameter where it entered the cell it is in
		Door *door = NULL;
		for (;;) {
			// The door's line, if the ray crosses it inside this cell where the door still stands
			Door *here = DoorAt(cellx, celly);
			if (here && rayy != 0) {
				float t = (celly + 0.5f - position.y) / rayy;
				float along = position.x + t * rayx - cellx;
				if (t >= z && t < MIN(sidex, sidey) && along >= here->open) {
					door = here;
					z = t;
					alongx = false;
					break;
				}
			}
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

		// Find the columns where the ray met the wall: the light tile's along the grid, so
		// light tiles join from cell to cell, and the texture's turned so no wall reads mirrored
		// A door carries its texture as it slides
		float hit = alongx ? position.y + z * rayy : position.x + z * rayx;
		float across = fract(hit) - (door ? door->open : 0);
		int lightu = CLAMP(across * TILE_SIZE, 0, TILE_SIZE);
		int u = (alongx && rayx < 0) || (!alongx && rayy > 0) ? TILE_SIZE - 1 - lightu : lightu;
		int walltile = door ? DOOR_TILE : Walls[cellx][celly] - 1;
		int walllight = WallLight(cellx, celly, alongx ? cellx - stepx : cellx, alongx ? celly : celly - stepy);
		float wallfade = LIGHT_NEAR * MIN(LIGHT_REACH / z, 1.0f) * (alongx ? RAYCAST_SIDE : 1.0f);
		float height = focal / z;
		float top = horizon - (1 - eye) * height;
		float bottom = horizon + eye * height;

		unsigned char *pixel = buffer + x;
		for (int y = 0; y < RETRO_HEIGHT; y++, pixel += RETRO_WIDTH) {
			if (y + 0.5f >= top && y + 0.5f < bottom) {
				int v = CLAMP((y + 0.5f - top) / height * TILE_SIZE, 0, TILE_SIZE);
				*pixel = Lit(SheetTexel(Textures, walltile, u, v), wallfade, SheetTexel(LightMaps, walllight, lightu, v));
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
			int light = below ? FloorLight(mapx, mapy, floory - mapy) : CeilingLights[mapx][mapy];
			float fade = LIGHT_NEAR * MIN(LIGHT_REACH / distance, 1.0f);
			*pixel = Lit(SheetTexel(Textures, tile, tx, ty), fade, SheetTexel(LightMaps, light, tx, ty));
		}
	}

	// Print the camera's position
	char text[32];
	snprintf(text, sizeof(text), "x %5.2f  y %5.2f", position.x, position.y);
	RETRO_PutString(text, TEXT_X + 1, TEXT_Y + 1, ShadowColor);
	RETRO_PutString(text, TEXT_X, TEXT_Y, TextColor);
}

void DEMO_Initialize(void)
{
	RETRO_Image *textures = RETRO_LoadImage("assets/raycast_walls_320x200.pcx", true);
	RETRO_Image *lightmaps = RETRO_LoadImage("assets/raycast_litemaps_320x200.pcx");
	Textures = textures->data;
	LightMaps = lightmaps->data;

	// Init light table, every entry of the sheet's palette at every level
	RETRO_CreateShadeTable(textures->palette, RETRO_COLORS, LIGHTS, LightTable);
	TextColor = RETRO_NearestPaletteIndex(RETRO_WHITE, textures->palette);
	ShadowColor = RETRO_NearestPaletteIndex(RETRO_BLACK, textures->palette);
}
