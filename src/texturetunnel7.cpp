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
// this one is the mesh, because a polar map about one centre cannot
// offset each ring from the last.
//
// The path is
//
//   p(t) = (Ax sin(ωx t),  Ay sin(ωy t + φ),  t)
//
// Always advancing in z, waving in x and y. The ring frame is the
// camera's own: forward is the tangent, right is world-down × forward,
// down is forward × right, the y-down, z-away frame RETRO_ViewVertex
// already assumes. Inward normals are −(cos θ · right + sin θ · down).
//
// Each quad is one brick of a generated tile. The tile is a grouted
// rectangle with a triangle pointing in +v, so once v runs down the
// tube the triangles point at the far end. Lighting is Lambert in view
// space, a light from the right, interpolated Gouraud through a shade
// table so the same texel is dark red-brown on the inside of the bend
// and bright orange on the outside. Shade is affine; UV is
// perspective-correct. RING_NEAR keeps the closest station in front of
// the near plane, so the drawers never see q = 0.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
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
#define GROUT 3 // texels of mortar on the leading edges; the trailing edges are brick so neighbours do not double it
#define FOG_SHADES 32
#define AMBIENT 0.46f // unlit walls stay a dark red, not black
#define FOG_KEEP 0.62f // far-end shade as a fraction of the lit shade

#define BRIGHT_ORANGE RETRO_Palette{ 255, 86, 6 }

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

// The axis at t, and the orthonormal frame a ring there is built in.
static Vertex Path(float t)
{
	return { { BEND_X * sinf(t * BEND_FX), BEND_Y * sinf(t * BEND_FY + BEND_PHASE), t } };
}

// Derivative of Path. z' is 1, so the tangent never parallels world
// down and the cross that makes right does not collapse.
static UnitVector PathTangent(float t)
{
	UnitVector forward = { { BEND_X * BEND_FX * cosf(t * BEND_FX), BEND_Y * BEND_FY * cosf(t * BEND_FY + BEND_PHASE), 1.0f } };
	return RETRO_NormalizeUnitVector(forward);
}

// Two axes perpendicular to forward, matching the camera's own right/down
// convention (retrocamera.h) so this doubles as both the camera's frame
// (forward points toward the look-ahead target) and a ring's cross-section frame (forward =
// PathTangent(along)). Takes forward itself rather than t, so a caller
// that already has it - the camera does, for its own forward - is not
// made to compute PathTangent twice for the same station. world-down is
// only ever the reference used to build right - it does not appear in
// the result, so the frame does not inherit a roll from it, only an
// orientation.
static void PathAxes(UnitVector forward, UnitVector *right, UnitVector *down)
{
	UnitVector worlddown = { { 0, 1, 0 } };
	*right = RETRO_UnitCrossProduct(worlddown, forward);
	*down = RETRO_UnitCrossProduct(forward, *right);
}

static void BuildBrick(void)
{
	// 4×4 Bayer. The triangle is mixed with the brick rather than filled
	// solid, which is the dithered mortar-and-wedge look of the reference.
	static const int bayer[4][4] = {
		{ 0, 8, 2, 10 },
		{ 12, 4, 14, 6 },
		{ 3, 11, 1, 9 },
		{ 15, 7, 13, 5 }
	};

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
				int dither = bayer[y & 3][x & 3];
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

static void DrawQuad(const RingVertex &a, const RingVertex &b, const RingVertex &c, const RingVertex &d)
{
	if (a.vertex.q <= 0.0f || b.vertex.q <= 0.0f || c.vertex.q <= 0.0f || d.vertex.q <= 0.0f) {
		return;
	}

	// a near-left, b far-left, c far-right, d near-right. That winding
	// points the inward normal at the camera. v grows toward the far ring.
	PolygonPoint point[4] = {
		{ a.vertex.spos, a.shade, { 0, 0 }, a.vertex.q },
		{ b.vertex.spos, b.shade, { 0, (float)TEXTURE_SIZE }, b.vertex.q },
		{ c.vertex.spos, c.shade, { (float)TEXTURE_SIZE, (float)TEXTURE_SIZE }, c.vertex.q },
		{ d.vertex.spos, d.shade, { (float)TEXTURE_SIZE, 0 }, d.vertex.q }
	};
	RETRO_DrawTexMapGouraudPolygon(point, 4, Brick, TEXTURE_SIZE, TEXTURE_SIZE, BrickShadeTable);
}

void DEMO_Render(double time, double deltatime)
{
	float t = (float)(time * FLIGHT_SPEED) + FLIGHT_START;

	// Stay on the path, but aim ahead so the viewer looks into each bend.
	Vertex origin = Path(t);
	Vertex target = Path(t + CAMERA_LOOKAHEAD);
	UnitVector forward = CAMERA_LOOKAHEAD > 0
		? RETRO_NormalizeUnitVector({ target.pos - origin.pos })
		: PathTangent(t);
	UnitVector right, down;
	PathAxes(forward, &right, &down);

	RETRO_Camera camera;
	RETRO_PlaceCamera(&camera, origin.pos, right, down, forward);

	// World stations, not camera-relative offsets: ring k is always at
	// k · spacing on the path. first is the nearest station still at
	// least RING_NEAR in front of the eye.
	int first = (int)ceilf((t + RING_NEAR) / RING_SPACING);
	float far = RING_SPACING * RING_COUNT;

	for (int i = 0; i < RING_COUNT; i++) {
		float along = (first + i) * (float)RING_SPACING;
		Vertex center = Path(along);
		UnitVector ringright, ringdown;
		PathAxes(PathTangent(along), &ringright, &ringdown);

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
			Vertex alongright = RETRO_ScaleUnitVector(ringright, ct);
			Vertex alongdown = RETRO_ScaleUnitVector(ringdown, st);
			Vertex radial = RETRO_AddVertex(alongright, alongdown);
			Vertex wall = RETRO_ScaleVertex(radial, TUNNEL_RADIUS);

			RingVertex *p = &Ring[i][s];
			p->vertex = RETRO_AddVertex(center, wall);

			RETRO_ViewVertex(&p->vertex, &camera);
			RETRO_ProjectViewVertex(&p->vertex);

			// Lambert lighting: the wall's inward normal (back toward the
			// tube's centre, so -radial) is carried into view space and
			// dotted with the fixed view-space light. lit is that term
			// eased through the shade table and floored at AMBIENT so the
			// unlit side never goes black; depth folds in the distance fog
			// on top, and the product is scaled into the shade table's
			// [0, FOG_SHADES) range for the Gouraud drawer to interpolate.
			Vertex negradial = RETRO_ScaleVertex(radial, -1.0f);
			UnitVector inward = RETRO_NormalizeUnitVector({ negradial.pos });
			RETRO_ViewUnitVector(&inward, &camera);
			float lambert = RETRO_RotatedDot(inward, Light);
			float lit = AMBIENT + (1.0f - AMBIENT) * RETRO_ShadeFromLambert(MAX(lambert, 0.0f));
			p->shade = lit * depth * (FOG_SHADES - 1);
		}
	}

	RETRO_ClearDepthBuffer();

	for (int i = RING_COUNT - 2; i >= 0; i--) {
		for (int s = 0; s < RING_SIDES; s++) {
			int s1 = (s + 1) % RING_SIDES;
			DrawQuad(Ring[i][s], Ring[i + 1][s], Ring[i + 1][s1], Ring[i][s1]);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, BRIGHT_ORANGE, Palette);
	RETRO_SetPalette(Palette);
	RETRO_CreatePaletteShadeTable(Palette, RETRO_COLORS, FOG_SHADES, FogTable);

	BuildBrick();

	// Light travels leftward in view space, so the wall whose inward
	// normal points left (the right-hand wall) is the bright one. It is
	// given directly in view space rather than a world direction that
	// RETRO_ViewUnitVector would turn into one, and fixed relative to the
	// camera, so it is set up once here rather than redone every frame.
	Light = RETRO_LightSource(-0.98f, 0.06f, -0.18f);
}
