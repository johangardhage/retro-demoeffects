//
// Stencil / metal vectors
//
// Faces as masks, not as shaded surfaces. Each visible face is filled
// with a screen-space picture whose origin is the face's own top-left
//
//   C(x, y) = Picture(x − min sx,  y − min sy)
//
// so the picture is locked to that face and slides as the face moves.
// The picture is a stack of metallic bars, one raised-cosine highlight
// per bar, packed as a wrapping 256×256 map. A slow scroll of that map
// is added to the face origin so the bars crawl. The bars are constant
// along a row, so it is the vertical half of that offset the eye sees;
// the map is kept two-dimensional because the lookup is, and a picture
// with any horizontal detail would then crawl without another line.
// Depth is the usual q-buffer. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define ROTATION_SPEED 1.1f
#define METAL_SIZE 256
#define METAL_BAR 16 // rows a bar occupies
#define METAL_SCROLL 40 // texels of the map a second

static unsigned char Metal[METAL_SIZE * METAL_SIZE];
static Model3D *Cube;

void DrawStencilFace(Face *face, Model3D *model, int scroll)
{
	PolygonPoint point[RETRO_MAX_FACEVERTICES];
	float minx = RETRO_WIDTH;
	float miny = RETRO_HEIGHT;

	for (int j = 0; j < face->vertices; j++) {
		Vertex *vertex = &model->vertex[face->vertex[j]];
		point[j].x = vertex->sx;
		point[j].y = vertex->sy;
		point[j].q = vertex->q;
		minx = MIN(minx, vertex->sx);
		miny = MIN(miny, vertex->sy);
	}

	int ox = (int)minx + scroll;
	int oy = (int)miny + scroll / 2;

	for (int triangle = 1; triangle < face->vertices - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (determinant == 0.0f) {
			continue;
		}

		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / determinant;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) {
				continue;
			}
			int xstart = MAX((int)ceil(span[y].left - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			unsigned char *buffer = RETRO_FrameBuffer();
			for (int x = xstart; x < xend; x++) {
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					int u = WRAP(x - ox, METAL_SIZE);
					int v = WRAP(y - oy, METAL_SIZE);
					buffer[offset] = Metal[v * METAL_SIZE + u];
				}
				q += dqdx;
			}
		}
	}
}

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	static double scroll = 0;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED * 0.83f, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED * 0.61f, 2 * M_PI);
	scroll = fmod(scroll + deltatime * METAL_SCROLL, METAL_SIZE);

	RETRO_RotateModel(ax, ay, az, Cube);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Cube);
	RETRO_SortFaces(Cube);
	RETRO_ClearDepthBuffer();

	int iscroll = (int)scroll;
	for (int i = 0; i < Cube->drawfaces; i++) {
		DrawStencilFace(&Cube->face[Cube->drawface[i]], Cube, iscroll);
	}
}

void DEMO_Initialize(void)
{
	// Chrome bars: a raised-cosine highlight per METAL_BAR rows
	for (int y = 0; y < METAL_SIZE; y++) {
		float t = (y % METAL_BAR) / (float)METAL_BAR;
		int shade = 24 + 231 * (0.5f - 0.5f * cos(2 * M_PI * t));
		memset(Metal + y * METAL_SIZE, shade, METAL_SIZE);
	}

	RETRO_CreateGradientPalette(0, 80, RETRO_BLACK, RETRO_SIENNA);
	RETRO_CreateGradientPalette(80, 180, RETRO_SIENNA, RETRO_GOLD);
	RETRO_CreateGradientPalette(180, RETRO_COLORS, RETRO_GOLD, RETRO_WHITE);

	Cube = RETRO_Load3DModel("assets/cubequads.obj");
}
