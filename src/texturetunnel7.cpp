//
// Tunnel, bent
//
// A brick tube whose axis is a 3D curve, not a straight vanishing point.
// Rings sit at fixed stations k · spacing on the path, each in the plane
// perpendicular to the tangent there, and consecutive rings are a quad
// each side. The camera walks t along the same path and looks a little
// ahead into the bend, so station k slides toward
// the eye. The first drawn ring is the smallest k with k · spacing ≥
// t + RING_NEAR; the one that has just passed RING_NEAR is dropped and
// a new one is taken on at the far end. That recycle is what flies you
// through. A mesh locked to t + (i + 1) · spacing would keep every
// brick at a constant depth and only the sine window would change, which
// is a snake in front of the camera rather than a tunnel the camera
// travels. texturetunnel.cpp is the polar 1/r form of a straight tube;
// this one is the mesh, because a polar map about one center cannot
// offset each ring from the last.
//
// The path is
//
//   p(t) = (Ax sin(ωx t),  Ay sin(ωy t + φ),  t)
//
// Always advancing in z, waving in x and y. The ring frame is the
// camera's own: forward is the tangent, right is world-down × forward,
// down is forward × right, the camera's y-down, z-away frame. Inward normals are −(cos θ · right + sin θ · down).
//
// Each quad is one brick of a generated tile. The tile is a grouted
// rectangle with a triangle pointing in +v, so once v runs down the
// tube the triangles point at the far end. Lighting is Lambert in view
// space, a light from the right, interpolated Gouraud through a shade
// table so the same texel is dark red-brown on the inside of the bend
// and bright orange on the outside. Shade is affine; UV is
// perspective-correct. Each quad is clipped to the sides of the view and
// the near plane before it is drawn. RING_NEAR keeps the closest station
// in front of the eye, but looking ahead into a bend can still swing the
// near ring's wall past the near plane, and those quads are cut there
// rather than lost.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocamera.h"
#include "lib/retropoly.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define RING_SIDES 20
#define RING_COUNT 36 // rings in front of the eye
#define TUNNEL_RADIUS 48
#define RING_SPACING 14 // world units from one ring to the next
#define RING_NEAR 10 // nearest a station is allowed to the eye, in path units
#define BEND_X 95 // how far the axis wanders off x
#define BEND_Y 58
#define BEND_FX 0.007f // radians of that wander per world unit of t
#define BEND_FY 0.009f
#define BEND_PHASE 1.1f
#define FLIGHT_START ((float)M_PI / (2.0f * BEND_FX)) // t where x' = 0, so the opening looks along the tube
#define FLIGHT_SPEED 108 // world units of t a second
#define CAMERA_LOOKAHEAD 100 // path units ahead to aim into the bend; 0 follows the tangent
#define TEXTURE_SIZE 64
#define GROUT 3 // texels of mortar on the leading edges; the trailing edges are brick so neighbors do not double it
#define FOG_SHADES 32
#define AMBIENT 0.46f // unlit walls stay a dark red, not black
#define FOG_KEEP 0.62f // far-end shade as a fraction of the lit shade

#define BRIGHT_ORANGE RETRO_Palette{ 255, 86, 6 }

struct RingVertex {
	vec3 eye;		// In the camera's frame
	float shade;
};

static unsigned char Brick[TEXTURE_SIZE * TEXTURE_SIZE];
static unsigned char FogTable[RETRO_COLORS * FOG_SHADES];
static const RETRO_ShadeTable BrickShadeTable = { FogTable, RETRO_COLORS, FOG_SHADES };
// Toward the light, in the camera's frame. It travels leftward, so the wall
// whose inward normal points left (the right-hand wall) is the bright one. It
// is given in view space rather than as a world direction RETRO_ViewDirection
// would turn into one, so it stays fixed relative to the camera.
static const vec3 Light = normalize(vec3{ -0.98f, 0.06f, -0.18f });

// The axis at t, and the orthonormal frame a ring there is built in.
static vec3 Path(float t)
{
	return { BEND_X * sinf(t * BEND_FX), BEND_Y * sinf(t * BEND_FY + BEND_PHASE), t };
}

// Derivative of Path. z' is 1, so the tangent never parallels world
// down and the cross that makes right does not collapse.
static vec3 PathTangent(float t)
{
	return normalize(vec3{ BEND_X * BEND_FX * cosf(t * BEND_FX), BEND_Y * BEND_FY * cosf(t * BEND_FY + BEND_PHASE), 1.0f });
}

static void BuildBrick(void)
{
	// The triangle is mixed with the brick rather than filled solid, which
	// is the dithered mortar-and-wedge look of the reference.
	for (int y = 0; y < TEXTURE_SIZE; y++) {
		for (int x = 0; x < TEXTURE_SIZE; x++) {
			unsigned char color;
			if (x < GROUT || y < GROUT) {
				color = 64;
			} else {
				float nx = (x + 0.5f) / TEXTURE_SIZE;
				float ny = (y + 0.5f) / TEXTURE_SIZE;
				// Apex at +v, base at v = 0: the triangle points down the tube.
				bool wedge = nx > ny * 0.62f && nx < 1.0f - ny * 0.62f;
				int dither = RETRO_DitherLevel(x, y);
				if (wedge) {
					color = dither < 10 ? 102 : 148;
				} else {
					color = dither < 6 ? 196 : 238;
				}
			}
			Brick[y * TEXTURE_SIZE + x] = color;
		}
	}
}

// Clip a quad to the view and draw what is left
static void DrawQuad(const RETRO_CameraLens &lens, const RingVertex &a, const RingVertex &b, const RingVertex &c, const RingVertex &d)
{
	// a near-left, b far-left, c far-right, d near-right. That winding
	// points the inward normal at the camera. v grows toward the far ring.
	RETRO_CameraVertex corner[4] = {
		{ a.eye, { 0, 0 }, a.shade },
		{ b.eye, { 0, (float)TEXTURE_SIZE }, b.shade },
		{ c.eye, { (float)TEXTURE_SIZE, (float)TEXTURE_SIZE }, c.shade },
		{ d.eye, { (float)TEXTURE_SIZE, 0 }, d.shade }
	};
	PolygonPoint point[4 + RETRO_CAMERA_CLIP_PLANES];
	int points = RETRO_ClipProjectViewPolygon(lens, corner, 4, point);
	if (points >= 3) {
		RETRO_DrawTexMapGouraudPolygon(point, points, Brick, TEXTURE_SIZE, TEXTURE_SIZE, BrickShadeTable);
	}
}

void DEMO_Render(RETRO_Time time)
{
	float t = (float)(time.total * FLIGHT_SPEED) + FLIGHT_START;

	// Stay on the path, but aim ahead so the viewer looks into each bend.
	vec3 origin = Path(t);
	vec3 forward = CAMERA_LOOKAHEAD > 0 ? normalize(Path(t + CAMERA_LOOKAHEAD) - origin) : PathTangent(t);
	RETRO_Camera camera;
	RETRO_LookAlong(&camera, origin, forward);

	// World stations, not camera-relative offsets: ring k is always at
	// k · spacing on the path. first is the nearest station still at
	// least RING_NEAR in front of the eye.
	int first = (int)ceilf((t + RING_NEAR) / RING_SPACING);
	float far = RING_SPACING * RING_COUNT;
	RingVertex ring[RING_COUNT][RING_SIDES];

	for (int i = 0; i < RING_COUNT; i++) {
		float along = (first + i) * (float)RING_SPACING;
		vec3 center = Path(along);
		vec3 ringright, ringdown;
		RETRO_FrameFromForward(PathTangent(along), &ringright, &ringdown);

		// fog is 1 at the near ring, falling toward 0 at the far one. depth
		// eases that into [FOG_KEEP, 1] by squaring (1 - fog) instead of fog
		// itself, so the slope is shallow near the camera and steepens
		// toward the far end - the near bricks stay crisp and the falloff
		// gathers pace as the tube recedes, rather than fading at a
		// constant rate throughout.
		float fog = 1.0f - (along - t) / far;
		float ease = 1.0f - fog;
		float depth = 1.0f - (1.0f - FOG_KEEP) * ease * ease;

		for (int s = 0; s < RING_SIDES; s++) {
			// theta walks one full turn around the ring; ct, st are its
			// position in the ring's own right/down plane.
			float theta = s * (2.0f * (float)M_PI / RING_SIDES);
			float ct = cosf(theta);
			float st = sinf(theta);

			// radial is the unit outward direction at this angle (ct, st is
			// already on the unit circle, and ringright/ringdown are
			// orthonormal, so the sum is unit with no renormalizing needed).
			// wall is that direction carried out to the tube wall, and
			// center + wall is where this vertex actually sits in the world.
			vec3 radial = ringright * ct + ringdown * st;
			vec3 wall = radial * TUNNEL_RADIUS;

			RingVertex *p = &ring[i][s];
			p->eye = RETRO_ViewPoint(&camera, center + wall);

			// Lambert lighting: the wall's inward normal (back toward the
			// tube's center, so -radial) is carried into view space and
			// dotted with the fixed view-space light. lit is that term
			// eased through the shade table and floored at AMBIENT so the
			// unlit side never goes black; depth folds in the distance fog
			// on top, and the product is scaled into the shade table's
			// [0, FOG_SHADES) range for the Gouraud drawer to interpolate.
			vec3 inward = RETRO_ViewDirection(&camera, normalize(-radial));
			float lambert = dot(inward, Light);
			float lit = mix(AMBIENT, 1.0f, RETRO_ShadeFractionFromLambert(MAX(lambert, 0.0f)));
			p->shade = lit * depth * (FOG_SHADES - 1);
		}
	}

	RETRO_ClearDepthBuffer();

	for (int i = RING_COUNT - 2; i >= 0; i--) {
		for (int s = 0; s < RING_SIDES; s++) {
			int s1 = (s + 1) % RING_SIDES;
			DrawQuad(camera.lens, ring[i][s], ring[i + 1][s], ring[i + 1][s1], ring[i][s1]);
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
}
