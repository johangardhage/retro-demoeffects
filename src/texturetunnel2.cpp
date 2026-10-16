//
// Tunnel, mirrored
//
// A straight textured tube made of rings of pixels around the middle of the
// screen.
//
// Every pixel belongs to one ring, its distance r from the middle, and has an
// angle a around it. Ring r is the wall at depth
//
//   depth = PERSPECTIVE / r     texture rows
//
// so the rings near the middle are far away and the wall nears toward the
// edges of the screen. A pixel shows the texel at row depth + t and
// column a + t, t = SPEED x seconds, so the wall flies at you SPEED rows a
// second and turns SPEED columns a second. Inside the first ring is the black
// hole at the far end.
//
// The texture is 256 x 128, two copies of a 128 x 128 tile side by side, and
// the 128 angles around the tube go round the tile once. It is mirrored below
// itself as it is looked up, so the rows run 256 before they repeat and the
// tube never shows a seam along it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

constexpr double SPEED = 70.0;                         // rows the wall flies and columns it turns a second, one a VGA frame

constexpr int TEXTURE_WIDTH = 256;
constexpr int TEXTURE_HEIGHT = 128;
constexpr int ROWS = 2 * TEXTURE_HEIGHT;   // the texture and its mirror image
constexpr int ANGLES = 128;                // texture columns around the tube

constexpr int FIRST = 14;                  // the far end, the first ring drawn
constexpr int PERSPECTIVE = 1949;          // rows of depth at a ring of one pixel
constexpr int HOLE = -1;                   // the depth of a pixel inside the first ring

static int DepthTable[RETRO_HEIGHT][RETRO_WIDTH];
static unsigned char AngleTable[RETRO_HEIGHT][RETRO_WIDTH];

//
// The depth and angle of every pixel. A pixel is measured from its middle, so
// the rings are round about the middle of the screen rather than about the
// pixel right of and below it
//
static void CreateTables(void)
{
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double fx = x - RETRO_WIDTH / 2 + 0.5;
			double fy = y - RETRO_HEIGHT / 2 + 0.5;
			int r = (int)floor(sqrt(fx * fx + fy * fy));
			DepthTable[y][x] = r < FIRST ? HOLE : PERSPECTIVE / r;
			AngleTable[y][x] = WRAP(atan2(fy, fx) * ANGLES / (2 * M_PI), ANGLES);
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	int travel = (int)(time.total * SPEED);
	const unsigned char *texture = RETRO_ImageData();
	unsigned char *screen = RETRO_FrameBuffer();
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int depth = DepthTable[y][x];
			if (depth == HOLE) continue;
			int v = WRAP(depth + travel, ROWS);
			if (v >= TEXTURE_HEIGHT) v = ROWS - 1 - v;
			screen[y * RETRO_WIDTH + x] = texture[v * TEXTURE_WIDTH + (travel + AngleTable[y][x]) % TEXTURE_WIDTH];
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/raindrops_256x128.pcx", true);
	CreateTables();
}
