//
// Tunnel, bend from moving rings
//
// A curved textured tube that snakes about, drawn as rings of pixels from the
// vanishing point out.
//
// Every pixel belongs to one ring, its distance r from the ring's center,
// and has an angle a around it. Ring r is the wall at depth
//
//   depth = PERSPECTIVE / r     texture rows
//
// so the rings near the middle are far away and the wall nears toward the
// edges of the screen. A pixel shows the texel at row depth + t and
// column a + t, t = SPEED x seconds, so the wall flies at you SPEED rows a
// second and turns SPEED columns a second. The wall is wobbly: the rings are
// pulled in and out by WOBBLE pixels in seven lobes around, so they lie in a
// wavy pattern.
//
// The pixels are kept ring by ring, so each ring can be drawn around a center
// of its own. The center of a ring runs around a circle of PATH_RADIUS
// pixels, a turn every PATH_TURN rows of depth, so the far end swings about
// the near one as the tunnel flies. A ring is drawn over and over along a
// line from the center of the ring inside it to its own, so it leaves no gaps
// between them. Rings are drawn from the far end out, each covering the one
// inside it, and what none of them reaches this frame is left as the last one
// drew it, so the tunnel is drawn over the frame before.
//
// The texture is 256 x 128, two copies of a 128 x 128 tile side by side, so
// the 256 angles around the tube go round the tile twice.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

constexpr double SPEED = 70.0;                         // rows the wall flies and columns it turns a second, one a VGA frame

constexpr int TEXTURE_WIDTH = 256;
constexpr int TEXTURE_HEIGHT = 128;
constexpr int ANGLES = 256;                         // texture columns around the tube

constexpr int FIRST = 16;                  // the far end, the first ring drawn
constexpr int RINGS = 256;                 // one past the last ring, out past the screen's corners
constexpr int PERSPECTIVE = 1134;          // rows of depth at a ring of one pixel
constexpr int WOBBLE = 4;                  // pixels the wall is pulled in and out
constexpr int LOBES = 7;
constexpr int PATH_RADIUS = 15;
constexpr int PATH_TURN = 128;             // rows of depth over which the path turns

// The pixels are made for every offset a center on the path can reach the
// screen from
constexpr int REACH_X = RETRO_WIDTH / 2 + PATH_RADIUS + 1;
constexpr int REACH_Y = RETRO_HEIGHT / 2 + PATH_RADIUS + 1;
constexpr int MAX_PIXELS = (2 * REACH_X) * (2 * REACH_Y);

struct Pixel {
	short x, y;                            // from the center of its ring
	unsigned char angle;
};

static Pixel Pixels[MAX_PIXELS];
static int RingStart[RINGS + 1];           // a ring's pixels are RingStart[r] up to RingStart[r + 1]

//
// The ring and angle of a pixel at (x, y) from the center. The pixel is
// measured from its middle, so the rings are round about the center rather
// than about the pixel right of and below it
//
static void Measure(int x, int y, int *ring, int *angle)
{
	double fx = x + 0.5;
	double fy = y + 0.5;
	double a = atan2(fy, fx);
	*ring = (int)floor(sqrt(fx * fx + fy * fy) - WOBBLE * sin(LOBES * a));
	*angle = WRAP(a * ANGLES / (2 * M_PI), ANGLES);
}

//
// Sorts every pixel into its ring, counting the pixels of each ring first
// so they can be laid out ring after ring
//
static void CreateRings(void)
{
	for (int y = -REACH_Y; y < REACH_Y; y++) {
		for (int x = -REACH_X; x < REACH_X; x++) {
			int r, a;
			Measure(x, y, &r, &a);
			if (r >= FIRST && r < RINGS) RingStart[r + 1]++;
		}
	}
	for (int r = 0; r < RINGS; r++) {
		RingStart[r + 1] += RingStart[r];
	}

	int next[RINGS];
	memcpy(next, RingStart, sizeof(next));
	for (int y = -REACH_Y; y < REACH_Y; y++) {
		for (int x = -REACH_X; x < REACH_X; x++) {
			int r, a;
			Measure(x, y, &r, &a);
			if (r >= FIRST && r < RINGS) {
				Pixels[next[r]++] = { (short)x, (short)y, (unsigned char)a };
			}
		}
	}
}

//
// Where the center of a ring is from the middle of the screen, travel texels
// into the flight
//
static void Center(int ring, int travel, int *x, int *y)
{
	int depth = PERSPECTIVE / ring + travel;
	double angle = (double)depth * RETRO_ANGLES_PER_TURN / PATH_TURN;
	*x = (int)lround(PATH_RADIUS * COS(angle));
	*y = (int)lround(PATH_RADIUS * SIN(angle));
}

static void DrawRing(int ring, int cx, int cy, int travel)
{
	unsigned char *screen = RETRO_FrameBuffer();
	const unsigned char *row = RETRO_ImageData() + WRAP(PERSPECTIVE / ring + travel, TEXTURE_HEIGHT) * TEXTURE_WIDTH;
	for (int i = RingStart[ring]; i < RingStart[ring + 1]; i++) {
		const Pixel &pixel = Pixels[i];
		int x = cx + pixel.x;
		int y = cy + pixel.y;
		if (x < 0 || x >= RETRO_WIDTH || y < 0 || y >= RETRO_HEIGHT) continue;
		screen[y * RETRO_WIDTH + x] = row[(travel + pixel.angle) % TEXTURE_WIDTH];
	}
}

//
// Draws the rings from the far end out, each along the line from the center
// of the ring inside it to its own. The line takes a pixel step at a time and
// draws on both sides of a diagonal step, so the ring never jumps a pixel
//
static void DrawTunnel(int travel)
{
	unsigned char *screen = RETRO_FrameBuffer();
	int px, py;
	Center(FIRST, travel, &px, &py);
	int x = RETRO_WIDTH / 2 + px;
	int y = RETRO_HEIGHT / 2 + py;

	// The hole at the far end, inside the first ring
	for (int hy = y - FIRST; hy < y + FIRST; hy++) {
		for (int hx = x - FIRST; hx < x + FIRST; hx++) {
			screen[hy * RETRO_WIDTH + hx] = 0;
		}
	}

	for (int r = FIRST; r < RINGS; r++) {
		int nx, ny;
		Center(r, travel, &nx, &ny);
		int dx = abs(nx - px);
		int dy = abs(ny - py);
		int sx = nx < px ? -1 : 1;
		int sy = ny < py ? -1 : 1;
		int major = MAX(dx, dy);
		int minor = MIN(dx, dy);
		int error = 0;
		for (int i = 0; i < major; i++) {
			DrawRing(r, x, y, travel);
			if (dx > dy) {
				x += sx;
			} else {
				y += sy;
			}
			error += minor;
			if (error >= major) {
				DrawRing(r, x, y, travel);
				if (dx > dy) {
					y += sy;
				} else {
					x += sx;
				}
				error -= major;
			}
		}
		DrawRing(r, x, y, travel);
		px = nx;
		py = ny;
	}
}

void DEMO_Render2(RETRO_Time time)
{
	DrawTunnel((int)(time.total * SPEED));
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/swirls_256x128.pcx", true);
	CreateRings();
}
