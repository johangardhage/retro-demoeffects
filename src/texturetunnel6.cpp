//
// Tunnel, fixed bend
//
// Reuses texturetunnel6's ring mesh, perspective-correct textured quads,
// generated triangle tile and Gouraud shade table. The camera and bend
// stay fixed; scrolling the wrapping V coordinate gives forward travel.
// Geometry and lighting are built once, leaving only the textured draw
// for each frame. The right wall glows orange and the left stays dark red.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrocamera.h"
#include "lib/retropoly.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define RING_SIDES 32
#define RING_COUNT 64
#define TUNNEL_RADIUS 48
#define RING_SPACING 8
#define RING_NEAR 10
#define BEND_CURVE 0.0025f
#define FLIGHT_SPEED 72
#define TEXTURE_SIZE 64
#define FOG_SHADES 32
#define AMBIENT 0.40f
#define FOG_KEEP 0.92f

#define BRIGHT_ORANGE RETRO_Palette{ 255, 72, 0 }

struct RingVertex {
	Vertex vertex;
	float shade;
};

static RingVertex Ring[RING_COUNT][RING_SIDES];
static unsigned char Brick[TEXTURE_SIZE * TEXTURE_SIZE];
static unsigned char FogTable[RETRO_COLORS * FOG_SHADES];
static RETRO_Palette Palette[RETRO_COLORS];
static ShadeTable BrickShadeTable = { FogTable, RETRO_COLORS, FOG_SHADES };
static UnitVector Light;

// A single left bend continues out of sight instead of waving back.
static Vertex Path(float t)
{
	return { { -BEND_CURVE * t * t, 0, t } };
}

static UnitVector PathTangent(float t)
{
	UnitVector forward = { { -2.0f * BEND_CURVE * t, 0, 1.0f } };
	return RETRO_NormalizeUnitVector(forward);
}

static void BuildBrick(void)
{
	// A dark triangle on an orange tile. Dither after projection so the
	// pattern stays at screen-pixel scale instead of aliasing in the distance.
	for (int y = 0; y < TEXTURE_SIZE; y++) {
		for (int x = 0; x < TEXTURE_SIZE; x++) {
			float nx = (x + 0.5f) / TEXTURE_SIZE;
			float ny = (y + 0.5f) / TEXTURE_SIZE;
			bool wedge = nx > ny * 0.5f && nx < 1.0f - ny * 0.5f;
			Brick[y * TEXTURE_SIZE + x] = wedge ? 128 : 248;
		}
	}
}

static void DrawQuad(const RingVertex &a, const RingVertex &b, const RingVertex &c, const RingVertex &d, float scroll)
{
	if (a.vertex.q <= 0.0f || b.vertex.q <= 0.0f || c.vertex.q <= 0.0f || d.vertex.q <= 0.0f) {
		return;
	}

	PolygonPoint point[4] = {
		{ a.vertex.spos, a.shade, { 0, scroll }, a.vertex.q },
		{ b.vertex.spos, b.shade, { 0, TEXTURE_SIZE + scroll }, b.vertex.q },
		{ c.vertex.spos, c.shade, { (float)TEXTURE_SIZE, TEXTURE_SIZE + scroll }, c.vertex.q },
		{ d.vertex.spos, d.shade, { (float)TEXTURE_SIZE, scroll }, d.vertex.q }
	};
	RETRO_DrawTexMapGouraudPolygon(point, 4, Brick, TEXTURE_SIZE, TEXTURE_SIZE, BrickShadeTable, true);
}

static void BuildTunnel(void)
{
	float t = 0.0f;

	Vertex origin = Path(t);
	UnitVector forward = PathTangent(t);
	UnitVector right, down;
	RETRO_FrameFromForward(forward, &right, &down);

	RETRO_Camera camera;
	RETRO_PlaceCamera(&camera, origin.pos, right, down, forward);

	int first = (int)ceilf((t + RING_NEAR) / RING_SPACING);
	float far = RING_SPACING * RING_COUNT;

	for (int i = 0; i < RING_COUNT; i++) {
		float along = (first + i) * (float)RING_SPACING;
		Vertex center = Path(along);
		UnitVector ringright, ringdown;
		RETRO_FrameFromForward(PathTangent(along), &ringright, &ringdown);

		float fog = 1.0f - (along - t) / far;
		float ease = 1.0f - fog;
		float depth = 1.0f - (1.0f - FOG_KEEP) * ease * ease;

		for (int s = 0; s < RING_SIDES; s++) {
			float theta = s * (2.0f * (float)M_PI / RING_SIDES);
			float ct = cosf(theta);
			float st = sinf(theta);

			Vertex alongright = RETRO_ScaleUnitVector(ringright, ct);
			Vertex alongdown = RETRO_ScaleUnitVector(ringdown, st);
			Vertex radial = RETRO_AddVertex(alongright, alongdown);
			Vertex wall = RETRO_ScaleVertex(radial, TUNNEL_RADIUS);

			RingVertex *p = &Ring[i][s];
			p->vertex = RETRO_AddVertex(center, wall);

			RETRO_ViewVertex(&p->vertex, &camera);
			RETRO_ProjectViewVertex(&p->vertex, 180, RETRO_WIDTH * 0.70f, RETRO_HEIGHT * 0.5f);

			Vertex negradial = RETRO_ScaleVertex(radial, -1.0f);
			UnitVector inward = RETRO_NormalizeUnitVector({ negradial.pos });
			RETRO_ViewUnitVector(&inward, &camera);
			float lambert = RETRO_RotatedDot(inward, Light);
			float lit = AMBIENT + (1.0f - AMBIENT) * RETRO_ShadeFractionFromLambert(MAX(lambert, 0.0f));
			p->shade = lit * depth * (FOG_SHADES - 1);
		}
	}

}

void DEMO_Render(double time, double deltatime)
{
	float scroll = fmod(time * FLIGHT_SPEED / RING_SPACING, 1.0) * TEXTURE_SIZE;
	RETRO_ClearDepthBuffer();

	for (int i = RING_COUNT - 2; i >= 0; i--) {
		for (int s = 0; s < RING_SIDES; s++) {
			int s1 = (s + 1) % RING_SIDES;
			DrawQuad(Ring[i][s], Ring[i + 1][s], Ring[i + 1][s1], Ring[i][s1], scroll);
		}
	}
	// Ordered two-by-two dithering into sixteen orange intensity levels.
	static const int threshold[2][2] = { { 0, 2 }, { 3, 1 } };
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;
			int value = RETRO.framebuffer[offset];
			int level = value / 17;
			if ((value % 17) * 4 > threshold[y & 1][x & 1] * 17 + 8) level++;
			RETRO.framebuffer[offset] = MIN(level, 15) * 17;
		}
	}

}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, BRIGHT_ORANGE, Palette);
	RETRO_SetPalette(Palette);
	RETRO_CreateShadeTable(Palette, RETRO_COLORS, FOG_SHADES, FogTable);

	BuildBrick();

	Light = RETRO_LightSource(-0.7f, 0.0f, -0.7f);
	BuildTunnel();
}
