//
// Tunnel, fixed bend
//
// Reuses texturetunnel7's ring mesh, perspective-correct textured quads,
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
	vec3 eye;		// In the camera's frame
	float shade;
};

static RingVertex Ring[RING_COUNT][RING_SIDES];
static unsigned char Brick[TEXTURE_SIZE * TEXTURE_SIZE];
static unsigned char FogTable[RETRO_COLORS * FOG_SHADES];
static const RETRO_ShadeTable BrickShadeTable = { FogTable, RETRO_COLORS, FOG_SHADES };
static const vec3 Light = normalize(vec3{ -0.7f, 0.0f, -0.7f }); // toward the light, in the camera's frame
static RETRO_CameraLens Lens;

// A single left bend continues out of sight instead of waving back.
static vec3 Path(float t)
{
	return { -BEND_CURVE * t * t, 0, t };
}

static vec3 PathTangent(float t)
{
	return normalize(vec3{ -2.0f * BEND_CURVE * t, 0, 1.0f });
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

// Clip a quad to the view and draw what is left
static void DrawQuad(const RingVertex &a, const RingVertex &b, const RingVertex &c, const RingVertex &d, float scroll)
{
	RETRO_CameraVertex corner[4] = {
		{ a.eye, { 0, scroll }, a.shade },
		{ b.eye, { 0, TEXTURE_SIZE + scroll }, b.shade },
		{ c.eye, { (float)TEXTURE_SIZE, TEXTURE_SIZE + scroll }, c.shade },
		{ d.eye, { (float)TEXTURE_SIZE, scroll }, d.shade }
	};
	RETRO_ProjectedPolygon projected;
	RETRO_CameraClipProject(Lens, corner, 4, &projected);
	if (projected.count >= 3) {
		RETRO_DrawTexMapGouraudPolygon(projected.point, projected.count, Brick, TEXTURE_SIZE, TEXTURE_SIZE, BrickShadeTable, true);
	}
}

static void BuildTunnel(void)
{
	float t = 0.0f;

	RETRO_Camera camera;
	RETRO_LookAlong(&camera, Path(t), PathTangent(t));
	camera.lens.focalx = 180;
	camera.lens.focaly = 180;
	camera.lens.center = { RETRO_WIDTH * 0.70f, RETRO_HEIGHT * 0.5f };
	Lens = camera.lens;

	int first = (int)ceilf((t + RING_NEAR) / RING_SPACING);
	float far = RING_SPACING * RING_COUNT;

	for (int i = 0; i < RING_COUNT; i++) {
		float along = (first + i) * (float)RING_SPACING;
		vec3 center = Path(along);
		vec3 ringright, ringdown;
		RETRO_FrameFromForward(PathTangent(along), { 0, 1, 0 }, &ringright, &ringdown);

		float fog = 1.0f - (along - t) / far;
		float ease = 1.0f - fog;
		float depth = 1.0f - (1.0f - FOG_KEEP) * ease * ease;

		for (int s = 0; s < RING_SIDES; s++) {
			float theta = s * (2.0f * (float)M_PI / RING_SIDES);
			float ct = cosf(theta);
			float st = sinf(theta);

			vec3 radial = ringright * ct + ringdown * st;
			vec3 wall = radial * TUNNEL_RADIUS;

			RingVertex *p = &Ring[i][s];
			p->eye = RETRO_ViewPoint(&camera, center + wall);

			vec3 inward = RETRO_ViewDirection(&camera, normalize(-radial));
			float lambert = dot(inward, Light);
			float lit = mix(AMBIENT, 1.0f, RETRO_ShadeFractionFromLambert(MAX(lambert, 0.0f)));
			p->shade = lit * depth * (FOG_SHADES - 1);
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	float scroll = fract(time.total * FLIGHT_SPEED / RING_SPACING) * TEXTURE_SIZE;
	RETRO_ClearDepthBuffer();

	for (int i = RING_COUNT - 2; i >= 0; i--) {
		for (int s = 0; s < RING_SIDES; s++) {
			int s1 = (s + 1) % RING_SIDES;
			DrawQuad(Ring[i][s], Ring[i + 1][s], Ring[i + 1][s1], Ring[i][s1], scroll);
		}
	}

	// Ordered two-by-two dithering into sixteen orange intensity levels.
	static const int threshold[2][2] = { { 0, 2 }, { 3, 1 } };
	unsigned char *buffer = RETRO_FrameBuffer();
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;
			int value = buffer[offset];
			int level = value / 17;
			if ((value % 17) * 4 > threshold[y & 1][x & 1] * 17 + 8) level++;
			buffer[offset] = MIN(level, 15) * 17;
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Palette palette[RETRO_COLORS];
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, BRIGHT_ORANGE, palette);
	RETRO_SetPalette(palette);
	RETRO_CreateShadeTable(palette, RETRO_COLORS, FOG_SHADES, FogTable);

	BuildBrick();

	BuildTunnel();
}
